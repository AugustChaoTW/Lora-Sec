// E8 — Node A: SENDER (victim of ACK spoofing)
//
// Sends data toward gateway. Tracks sender-side PDR via ACK receipt.
// In the ACK spoofing attack:
//   - Routes via 0xCC (attacker poses as relay with metric=1)
//   - Sends data to 0xCC as next hop
//   - Receives forged ACK(src=0xBB) — believes delivery confirmed
//   - Sender-side "PDR" climbs toward 100%
//   - Gateway actually received 0 packets
//
// The discrepancy between sender-reported PDR and gateway PDR proves
// that ACK messages need HMAC protection.
//
// Key serial output to watch:
//   [RX] ACK id=N from 0xBB PDR=100.0%   ← spoofed ACK accepted
//   vs gateway log showing 0 received packets
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE  1
#define NODE_ID        0xAA
#define GATEWAY_ID     0xBB
#define DATA_PERIOD_MS  5000

#include "../e8_common.h"
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

uint16_t data_seq   = 0;
uint32_t pkt_sent   = 0;
uint32_t ack_recv   = 0;
uint32_t last_data  = 0;
uint32_t last_oled  = 0;
uint32_t hmac_drops = 0;
char     status[24] = "waiting Hello...";

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE A  ACK VICTIM");
  char buf[32];
  snprintf(buf, sizeof(buf), "NH=0x%02X m=%u", best_nh, best_metric);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, status);
  snprintf(buf, sizeof(buf), "PDR=%.0f%% (FAKE?)",
           pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
  oled.drawString(0, 42, buf);
  oled.display();
}

void send_data() {
  DataMsg d = {};
  d.type      = MSG_DATA;
  d.src       = NODE_ID;
  d.dst       = best_nh;
  d.final_dst = GATEWAY_ID;
  d.pkt_id    = data_seq++;
  snprintf((char*)d.payload, 8, "A%05u", d.pkt_id);
  sign_data(&d);
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  Serial.printf("[TX] DATA id=%u via=0x%02X (sender PDR=%.1f%%)\n",
                d.pkt_id, best_nh, pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
  if (best_nh == 0xCC)
    Serial.println("[WARN] Routing via 0xCC — may be ACK spoofer");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E8 Node A (SENDER — ACK spoof victim, SECURITY_MODE=1) ===");
  Serial.println("[INFO] Watch: sender PDR climbs to ~100% while gateway PDR=0%.");
  Serial.println("[INFO] Forged ACKs have no HMAC — indistinguishable from real ACKs.");

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
        hmac_drops++;
        Serial.printf("[DROP] Hello from 0x%02X: HMAC FAILED\n", h->src);
        return;
      }
      uint8_t cand = h->metric + 1;
      if (cand < best_metric || now - last_update > 30000) {
        best_nh = h->src; best_metric = cand; last_update = now;
        Serial.printf("[ROUTE] NH=0x%02X metric=%u\n", best_nh, best_metric);
        snprintf(status, sizeof(status), "NH=0x%02X m=%u", best_nh, best_metric);
        oled_draw();
      }

    } else if (buf[0] == MSG_ACK && pkt_len >= sizeof(AckMsg)) {
      AckMsg *a = (AckMsg*)buf;
      if (a->dst == NODE_ID) {
        ack_recv++;
        // Cannot verify ACK authenticity — no HMAC in AckMsg
        if (a->src == GATEWAY_ID) {
          Serial.printf("[RX] ACK id=%u from 0xBB (sender PDR=%.1f%%) — may be FORGED\n",
                        a->pkt_id, (float)ack_recv/pkt_sent*100);
        } else {
          Serial.printf("[RX] ACK id=%u from 0x%02X (sender PDR=%.1f%%)\n",
                        a->pkt_id, a->src, (float)ack_recv/pkt_sent*100);
        }
        snprintf(status, sizeof(status), "ACK id=%u (PDR?)", a->pkt_id);
        oled_draw();
      }
    }
  }
}
