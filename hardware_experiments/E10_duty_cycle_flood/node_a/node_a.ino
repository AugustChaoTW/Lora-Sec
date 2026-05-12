// E10 — Node A: SENDER with duty cycle tracking
//
// Tracks its own airtime consumption (tx_ms / elapsed_ms × 100%).
// When honest nodes respond to flood Hellos (route updates + re-broadcasts),
// their DC budget is consumed. This node logs when data TX is suppressed
// due to DC limit being exceeded.
//
// DC enforcement is SIMULATED here (firmware-level, not SX1262 hardware).
// LoRaMesher upstream does not implement DC tracking — this node adds it
// to measure the attack's impact.
//
// Key serial output to watch:
//   [DC] tx_ms=185 dc_used=18.5% (limit=1%) — BLOCKED
//   [TX] DATA BLOCKED — DC exhausted by flood-induced relay overhead
//   vs (without attacker):
//   [DC] tx_ms=185 dc_used=0.9% — OK
//   [TX] DATA id=N via=0xBB
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE     1
#define NODE_ID           0xAA
#define GATEWAY_ID        0xBB
#define DATA_PERIOD_MS     5000
#define DC_WINDOW_MS      60000   // measure DC over 60-second rolling window
#define DC_LIMIT_PCT       1      // AS923: 1% duty cycle limit

#include "../e10_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint8_t  best_nh      = GATEWAY_ID;
uint8_t  best_metric  = 255;
uint32_t last_update  = 0;

uint16_t data_seq     = 0;
uint32_t pkt_sent     = 0;
uint32_t pkt_blocked  = 0;
uint32_t ack_recv     = 0;
uint32_t last_data    = 0;
uint32_t last_oled    = 0;

// Duty cycle tracking
uint32_t window_start_ms  = 0;
uint32_t window_tx_ms     = 0;

void dc_record_tx(uint32_t tx_ms) {
  uint32_t now = millis();
  if (now - window_start_ms > DC_WINDOW_MS) {
    window_start_ms = now;
    window_tx_ms    = 0;
  }
  window_tx_ms += tx_ms;
}

float dc_current_pct() {
  uint32_t elapsed = millis() - window_start_ms;
  if (elapsed < 100) return 0;
  return (float)window_tx_ms / elapsed * 100.0f;
}

bool dc_can_transmit() {
  return dc_current_pct() < DC_LIMIT_PCT;
}

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  oled.drawString(0,  0, "NODE A  DC VICTIM");
  snprintf(buf, sizeof(buf), "DC=%.2f%% lim=%d%%", dc_current_pct(), DC_LIMIT_PCT);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "PDR=%.0f%% blk=%lu",
           pkt_sent ? (float)ack_recv/pkt_sent*100 : 0, pkt_blocked);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, dc_can_transmit() ? "TX OK" : "TX BLOCKED!");
  oled.display();
}

void send_data() {
  if (!dc_can_transmit()) {
    pkt_blocked++;
    Serial.printf("[TX] DATA BLOCKED — DC=%.2f%% > %d%% limit (blocked=%lu)\n",
                  dc_current_pct(), DC_LIMIT_PCT, pkt_blocked);
    oled_draw();
    return;
  }
  DataMsg d = {};
  d.type      = MSG_DATA;
  d.src       = NODE_ID;
  d.dst       = best_nh;
  d.final_dst = GATEWAY_ID;
  d.pkt_id    = data_seq++;
  snprintf((char*)d.payload, 8, "A%05u", d.pkt_id);
  sign_data(&d);
  uint32_t t0 = millis();
  radio->transmit((uint8_t*)&d, sizeof(d));
  uint32_t tx_ms = millis() - t0;
  radio->startReceive();
  dc_record_tx(tx_ms);
  pkt_sent++;
  Serial.printf("[TX] DATA id=%u via=0x%02X DC=%.2f%% PDR=%.1f%%\n",
                d.pkt_id, best_nh, dc_current_pct(),
                pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
}

void relay_hello_with_dc(const HelloMsg *src_hello) {
  if (!dc_can_transmit()) {
    Serial.printf("[RELAY] SUPPRESSED — DC=%.2f%% > %d%% (flood causing starvation)\n",
                  dc_current_pct(), DC_LIMIT_PCT);
    return;
  }
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = src_hello->metric + 1;
  h.seq    = src_hello->seq;
  sign_hello(&h);
  uint32_t t0 = millis();
  delay(200);
  radio->transmit((uint8_t*)&h, sizeof(h));
  uint32_t tx_ms = millis() - t0;
  radio->startReceive();
  dc_record_tx(tx_ms);
  Serial.printf("[RELAY] Hello metric=%u DC=%.2f%%\n", h.metric, dc_current_pct());
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E10 Node A (SENDER — DC tracking, SECURITY_MODE=1) ===");
  Serial.printf("[INFO] DC window=%ds limit=%d%%\n", DC_WINDOW_MS/1000, DC_LIMIT_PCT);
  Serial.println("[INFO] Data TX blocked when DC exceeds limit due to flood-induced relay traffic.");
  window_start_ms = millis();

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
    { last_oled = now; oled_draw();
      Serial.printf("[DC] current=%.2f%% window_tx=%lums\n",
                    dc_current_pct(), window_tx_ms); }

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
#if SECURITY_MODE >= 1
      if (!check_hello(h)) {
        Serial.printf("[DROP] Hello 0x%02X HMAC FAILED\n", h->src);
        return;
      }
#endif
      uint8_t cand = h->metric + 1;
      if (cand < best_metric || now - last_update > 30000) {
        best_nh = h->src; best_metric = cand; last_update = now;
        Serial.printf("[ROUTE] NH=0x%02X metric=%u\n", best_nh, best_metric);
      }
      // Each relay hello consumes DC
      relay_hello_with_dc(h);
      oled_draw();

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
