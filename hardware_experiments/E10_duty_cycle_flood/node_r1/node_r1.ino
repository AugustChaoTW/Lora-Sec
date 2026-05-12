// E7 — Relay Node R1 (honest node, Black Hole victim)
//
// Role: Re-broadcasts Hellos with metric+1, forwards Data toward gateway.
//       Also sends its own data as a sensor node.
//
// Configure for R2/R3 by changing NODE_ID below.
//
// SECURITY_MODE=1 (HMAC-SHA256 patched). This demonstrates that HMAC
// is insufficient against insider Black Hole: attacker C(0xCC) holds
// the valid PSK and its Hello passes check_hello() — relay node
// accepts it, updates route to NH=0xCC, and forwards data into black hole.
//
// Expected behavior when attacker C(0xCC) is active:
//   - Relay receives Hello from 0xCC (metric=0, valid HMAC) → ACCEPTED
//   - Relay updates NH=0xCC, metric=1
//   - Relay forwards all traffic to 0xCC → silently dropped
//   - Gateway sees no packets → PDR=0%
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// Topology: A(0xAA) → R1(0x11) → R2(0x22) → R3(0x33) → B(0xBB)
//           Attacker C(0xCC) poisons routes on all honest nodes

#define SECURITY_MODE  1    // MUST be 1 — demonstrates patched relay is still vulnerable
#define NODE_ID        0x11 // ← 0x11=R1, 0x22=R2, 0x33=R3
#define GATEWAY_ID     0xBB
#define DATA_PERIOD_MS  7000
#define RELAY_DELAY_MS   500

#include "../e10_common.h"
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

uint16_t data_seq    = 0;
uint16_t relay_seq   = 0;
uint32_t pkt_sent    = 0;
uint32_t ack_recv    = 0;
uint32_t relay_count = 0;
uint32_t drop_count  = 0;
uint32_t last_data   = 0;
uint32_t last_oled   = 0;
char     status[24]  = "waiting...";

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  snprintf(buf, sizeof(buf), "RELAY %02X  VICTIM", NODE_ID);
  oled.drawString(0,  0, buf);
  snprintf(buf, sizeof(buf), "NH=0x%02X m=%u", best_nh, best_metric);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, status);
  snprintf(buf, sizeof(buf), "FWD=%lu DROP=%lu", relay_count, drop_count);
  oled.drawString(0, 42, buf);
  oled.display();
}

void relay_hello(const HelloMsg *src_hello) {
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = src_hello->metric + 1;
  h.seq    = relay_seq++;
  sign_hello(&h);
  delay(RELAY_DELAY_MS);
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  relay_count++;
  Serial.printf("[RELAY] Re-broadcast Hello metric=%u (learned from 0x%02X metric=%u)\n",
                h.metric, src_hello->src, src_hello->metric);
}

void send_data() {
  DataMsg d = {};
  d.type      = MSG_DATA;
  d.src       = NODE_ID;
  d.dst       = best_nh;
  d.final_dst = GATEWAY_ID;
  d.pkt_id    = data_seq++;
  snprintf((char*)d.payload, 8, "%02X%04u", NODE_ID, d.pkt_id);
  sign_data(&d);
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  Serial.printf("[TX] DATA id=%u via=0x%02X\n", d.pkt_id, best_nh);
}

void forward_data(const DataMsg *incoming) {
  DataMsg fwd = *incoming;
  fwd.dst = best_nh;
  sign_data(&fwd);
  radio->transmit((uint8_t*)&fwd, sizeof(fwd));
  radio->startReceive();

  if (best_nh == 0xCC) {
    Serial.printf("[FWD→BLACKHOLE] DATA id=%u src=0x%02X → 0xCC (will be dropped!)\n",
                  fwd.pkt_id, fwd.src);
  } else {
    Serial.printf("[FWD] DATA id=%u src=0x%02X → 0x%02X\n",
                  fwd.pkt_id, fwd.src, fwd.dst);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E10 Relay 0x%02X (honest, SECURITY_MODE=1) ===\n", NODE_ID);
  Serial.println("[INFO] Demonstrates: insider Black Hole bypasses HMAC patch.");
  Serial.println("[INFO] This relay will accept 0xCC's Hello (valid HMAC) and route into black hole.");

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
  if (now - last_data >= DATA_PERIOD_MS && best_metric < 255)
    { last_data = now; send_data(); }
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

      if (!check_hello(h)) {
        drop_count++;
        Serial.printf("[DROP] Hello from 0x%02X: HMAC FAILED\n", h->src);
        snprintf(status, sizeof(status), "DROP 0x%02X HMAC", h->src);
        oled_draw();
        return;
      }

      // HMAC passed — attacker 0xCC reaches here too
      uint8_t cand_metric = h->metric + 1;
      if (cand_metric < best_metric || now - last_update > 30000) {
        best_nh     = h->src;
        best_metric = cand_metric;
        last_update = now;

        if (h->src == 0xCC) {
          Serial.printf("[ROUTE] NH=0xCC metric=%u — BLACK HOLE inserted (HMAC valid!)\n",
                        cand_metric);
          snprintf(status, sizeof(status), "NH=0xCC POISONED!");
        } else {
          Serial.printf("[ROUTE] NH=0x%02X metric=%u\n", best_nh, best_metric);
          snprintf(status, sizeof(status), "NH=%02X m=%u", best_nh, best_metric);
        }
      }
      relay_hello(h);
      oled_draw();

    } else if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      if (d->src == NODE_ID) return;
      if (d->dst == NODE_ID) {
        if (!check_data(d)) {
          drop_count++;
          Serial.printf("[DROP] DATA from 0x%02X: HMAC FAILED\n", d->src);
          return;
        }
        if (d->final_dst == GATEWAY_ID && best_metric < 255) {
          forward_data(d);
        }
      }

    } else if (buf[0] == MSG_ACK && pkt_len >= sizeof(AckMsg)) {
      AckMsg *a = (AckMsg*)buf;
      if (a->dst == NODE_ID) {
        ack_recv++;
        Serial.printf("[RX] ACK id=%u PDR=%.1f%%\n",
                      a->pkt_id, (float)ack_recv/pkt_sent*100);
      }
    }
  }
}
