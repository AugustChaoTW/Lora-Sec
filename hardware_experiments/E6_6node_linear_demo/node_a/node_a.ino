// E6 — Node A: SENDER (leaf node)
// Role: Sends data to Gateway via best known route.
//       Listens for Hellos and updates routing table.
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// SECURITY_MODE:
//   0 = no auth (baseline — accepts any Hello)
//   1 = HMAC-SHA256 (patched — rejects forged Hellos)

#define SECURITY_MODE  1   // ← change to 0 for baseline run
#define NODE_ID        0xAA
#define GATEWAY_ID     0xBB
#define DATA_PERIOD_MS  5000

#include "../e6_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

// Simple routing table: best next-hop to gateway
uint8_t  best_nh     = GATEWAY_ID;
uint8_t  best_metric = 255;
uint32_t last_update = 0;

uint16_t data_seq   = 0;
uint32_t pkt_sent   = 0;
uint32_t ack_recv   = 0;
uint32_t last_data  = 0;
uint32_t last_oled  = 0;
uint32_t drop_count = 0;
char     status[24] = "waiting Hello...";

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  snprintf(buf, sizeof(buf), "NODE A  MODE:%d", SECURITY_MODE);
  oled.drawString(0,  0, buf);
  snprintf(buf, sizeof(buf), "NH=0x%02X m=%u", best_nh, best_metric);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, status);
  snprintf(buf, sizeof(buf), "PDR=%.0f%% DROP=%lu",
           pkt_sent ? (float)ack_recv/pkt_sent*100 : 0, drop_count);
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
#if SECURITY_MODE >= 1
  sign_data(&d);
#endif
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  Serial.printf("[TX] DATA id=%u via=0x%02X PDR=%.1f%%\n",
                d.pkt_id, best_nh, pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E6 Node A (SENDER)  SECURITY_MODE=%d ===\n", SECURITY_MODE);
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

#if SECURITY_MODE >= 1
      if (!check_hello(h)) {
        drop_count++;
        Serial.printf("[DROP] Hello from 0x%02X: HMAC FAILED (total drops=%lu)\n",
                      h->src, drop_count);
        snprintf(status, sizeof(status), "DROP 0x%02X HMAC FAIL", h->src);
        oled_draw();
        return;
      }
#endif
      // Update route if better metric
      uint8_t candidate_metric = h->metric + 1;
      if (candidate_metric < best_metric ||
          (now - last_update > 30000)) {  // refresh after 30s
        best_nh     = h->src;
        best_metric = candidate_metric;
        last_update = now;
        Serial.printf("[ROUTE] Best NH=0x%02X metric=%u (via Hello from 0x%02X)\n",
                      best_nh, best_metric, h->src);
        snprintf(status, sizeof(status), "NH=0x%02X m=%u OK", best_nh, best_metric);
        oled_draw();
      }

    } else if (buf[0] == MSG_ACK && pkt_len >= sizeof(AckMsg)) {
      AckMsg *a = (AckMsg*)buf;
      if (a->dst == NODE_ID) {
        ack_recv++;
        Serial.printf("[RX] ACK id=%u PDR=%.1f%%\n",
                      a->pkt_id, (float)ack_recv/pkt_sent*100);
        snprintf(status, sizeof(status), "ACK id=%u OK", a->pkt_id);
        oled_draw();
      }
    }
  }
}
