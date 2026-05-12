// E7 — Node B: GATEWAY (honest node)
//
// Role: Broadcasts Hello(metric=0) to start route discovery.
//       Receives and ACKs Data packets from sender.
//       Tracks PDR — number of received packets / expected.
//
// In the Black Hole attack (E7), all data is routed to attacker 0xCC
// and silently dropped. Gateway receives 0 packets → PDR = 0%.
// This happens even though SECURITY_MODE=1 (HMAC patch is active),
// demonstrating that Authentication ≠ Availability.
//
// The gateway itself is honest and cannot detect the Black Hole:
//   - It never receives data (silence looks like link failure)
//   - It has no visibility into other nodes' routing tables
//   - HMAC only tells it that messages it DOES receive are authentic
//
// Expected serial output during attack:
//   [TX] Hello seq=N → waiting for data
//   (silence) — no [RX] DATA lines
//   At end: PDR=0% despite SECURITY_MODE=1
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE  1   // MUST be 1 — demonstrates patched gateway still sees PDR=0%
#define NODE_ID        0xBB
#define HELLO_PERIOD_MS 10000

#include "../e15_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t hello_seq   = 0;
uint32_t last_hello  = 0;
uint32_t pkt_recv    = 0;
uint32_t hello_count = 0;
uint32_t last_oled   = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE B  GATEWAY");
  char buf[32];
  snprintf(buf, sizeof(buf), "MODE: %d  Hello=%lu", SECURITY_MODE, hello_count);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "RX PKT: %lu", pkt_recv);
  oled.drawString(0, 28, buf);
  if (pkt_recv == 0 && hello_count > 3)
    oled.drawString(0, 42, "PDR=0% BLACK HOLE?");
  else
    oled.drawString(0, 42, "watching...");
  oled.display();
}

void send_hello() {
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = NODE_ID;
  h.metric = 0;
  h.seq    = hello_seq++;
  sign_hello(&h);
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  hello_count++;
  Serial.printf("[TX] Hello seq=%u (hello #%lu)\n", h.seq, hello_count);
  if (pkt_recv == 0 && hello_count > 3) {
    Serial.println("[WARN] No packets received after 3+ Hellos — Black Hole likely active");
    Serial.println("[WARN] SECURITY_MODE=1 cannot detect insider dropping behavior");
  }
}

void send_ack(uint8_t dst, uint16_t pkt_id) {
  AckMsg a = {};
  a.type   = MSG_ACK;
  a.src    = NODE_ID;
  a.dst    = dst;
  a.pkt_id = pkt_id;
  radio->transmit((uint8_t*)&a, sizeof(a));
  radio->startReceive();
  Serial.printf("[TX] ACK id=%u → 0x%02X\n", pkt_id, dst);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E15 Node B (GATEWAY — honest, SECURITY_MODE=1) ===");
  Serial.println("[INFO] Gateway broadcasts Hello. Insider attacker (0xCC) races with own Hello.");
  Serial.println("[INFO] All honest nodes route to 0xCC (Black Hole). PDR drops to 0%.");
  Serial.println("[INFO] This gateway cannot detect the Black Hole — it just observes silence.");

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

  // Send first Hello immediately
  delay(100);
  send_hello();
  last_hello = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - last_hello >= HELLO_PERIOD_MS) { last_hello = now; send_hello(); }
  if (now - last_oled  >= 5000)            { last_oled  = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      if (d->final_dst != NODE_ID) return;
      if (!check_data(d)) {
        Serial.printf("[DROP] DATA from 0x%02X: HMAC FAILED\n", d->src);
        return;
      }
      pkt_recv++;
      Serial.printf("[RX] DATA id=%u from 0x%02X payload=%.8s (total=%lu)\n",
                    d->pkt_id, d->src, d->payload, pkt_recv);
      oled_draw();
      send_ack(d->src, d->pkt_id);
    }
  }
}
