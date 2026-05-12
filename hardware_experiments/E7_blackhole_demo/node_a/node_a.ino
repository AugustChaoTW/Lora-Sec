// E7 — Node A: SENDER (honest node, Black Hole victim)
//
// Role: Sends data to Gateway via best known route.
//       Listens for Hellos and updates routing table.
//
// SECURITY_MODE=1 (HMAC-SHA256 patched) is the ONLY interesting mode here.
// With SECURITY_MODE=0 (baseline), the E6 attacker already works.
// The E7 finding requires SECURITY_MODE=1 to show the HMAC patch
// is insufficient against an insider (Black Hole) attacker.
//
// Expected behavior when attacker C(0xCC) is active:
//   - Node A receives Hello from 0xCC with valid HMAC → ACCEPTED
//   - Node A updates NH=0xCC, metric=1
//   - Node A sends data to 0xCC → data is silently dropped
//   - Node A never receives ACK → PDR=0%
//   - Serial shows: "NH=0xCC m=1 OK" then "TX DATA via=0xCC" with no ACKs
//
// FINDING: HMAC passes for insider attacker. Route poisoning succeeds.
//          Authentication ≠ Availability.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE  1   // MUST be 1 — E7 demonstrates patched node is still vulnerable
#define NODE_ID        0xAA
#define GATEWAY_ID     0xBB
#define DATA_PERIOD_MS  5000

#include "../e7_common.h"
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
uint32_t drop_count = 0;
char     status[24] = "waiting Hello...";

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  oled.drawString(0,  0, "NODE A  VICTIM");
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
  sign_data(&d);
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;

  if (best_nh == 0xCC) {
    Serial.printf("[TX] DATA id=%u via=0xCC (BLACK HOLE!) PDR=%.1f%%\n",
                  d.pkt_id, pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
    Serial.println("[WARN] Routing to attacker 0xCC — expect no ACK");
  } else {
    Serial.printf("[TX] DATA id=%u via=0x%02X PDR=%.1f%%\n",
                  d.pkt_id, best_nh, pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E7 Node A (SENDER — honest, SECURITY_MODE=1) ===");
  Serial.println("[INFO] Demonstrates: insider Black Hole bypasses HMAC patch.");
  Serial.println("[INFO] Watch for NH=0xCC after attacker broadcasts signed Hello.");
  Serial.println("[INFO] PDR should drop to 0% when routing to 0xCC.");

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
        Serial.printf("[DROP] Hello from 0x%02X: HMAC FAILED (total drops=%lu)\n",
                      h->src, drop_count);
        snprintf(status, sizeof(status), "DROP 0x%02X HMAC FAIL", h->src);
        oled_draw();
        return;
      }

      // HMAC passed — attacker's Hello reaches here too!
      uint8_t candidate_metric = h->metric + 1;
      if (candidate_metric < best_metric || (now - last_update > 30000)) {
        best_nh     = h->src;
        best_metric = candidate_metric;
        last_update = now;

        if (h->src == 0xCC) {
          Serial.printf("[ROUTE] NH=0xCC metric=%u — ATTACKER INSERTED (HMAC passed!)\n",
                        candidate_metric);
          Serial.println("[ROUTE] Black Hole attack SUCCESS: route table poisoned");
          snprintf(status, sizeof(status), "NH=0xCC POISONED!");
        } else {
          Serial.printf("[ROUTE] Best NH=0x%02X metric=%u\n", best_nh, best_metric);
          snprintf(status, sizeof(status), "NH=0x%02X m=%u OK", best_nh, best_metric);
        }
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
