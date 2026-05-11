// E6 — Node B: GATEWAY
// Role: Broadcasts Hello(metric=0), ACKs received Data.
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// SECURITY_MODE:
//   0 = no auth (baseline)
//   1 = HMAC-SHA256

#define SECURITY_MODE  1   // ← change to 0 for baseline run
#define NODE_ID        0xBB
#define HELLO_PERIOD_MS 10000

#include "../e6_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t hello_seq  = 0;
uint16_t mv         = 0;
uint32_t last_hello = 0;
uint32_t pkt_recv   = 0;
uint32_t last_oled  = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE B  GATEWAY");
  char buf[32];
  snprintf(buf, sizeof(buf), "MODE: %d", SECURITY_MODE);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "RX PKT: %lu", pkt_recv);
  oled.drawString(0, 28, buf);
  snprintf(buf, sizeof(buf), "MV: %u", mv);
  oled.drawString(0, 42, buf);
  oled.display();
}

void send_hello() {
  mv++;
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = NODE_ID;
  h.metric = 0;
  h.seq    = hello_seq++;
#if SECURITY_MODE >= 1
  sign_hello(&h);
#endif
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[TX] Hello mv=%u seq=%u\n", mv, h.seq);
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
  Serial.printf("\n=== E6 Node B (GATEWAY)  SECURITY_MODE=%d ===\n", SECURITY_MODE);
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
#if SECURITY_MODE >= 1
      if (!check_data(d)) {
        Serial.printf("[DROP] DATA from 0x%02X: HMAC FAILED\n", d->src);
        return;
      }
#endif
      pkt_recv++;
      Serial.printf("[RX] DATA id=%u from 0x%02X payload=%.8s (total=%lu)\n",
                    d->pkt_id, d->src, d->payload, pkt_recv);
      oled_draw();
      send_ack(d->src, d->pkt_id);
    }
  }
}
