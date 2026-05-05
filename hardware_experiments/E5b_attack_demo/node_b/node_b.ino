// E5b — Node B: GATEWAY (receiver)
// Role   : Gateway — advertises metric=0, receives data, sends ACK
// Port   : /dev/cu.usbmodem134201  MAC: D0:CF:13:0A:48:88
// Board  : Ebyte EoRa PI (ESP32-S3FH4R2 + E22-900MM22S / SX1262)
// FQBN   : esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE 0   // ← match Node A
#define NODE_ID       0xBB
#define HELLO_PERIOD_MS 10000

#include "../e5b_common.h"

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t hello_seq   = 0;
uint32_t last_hello  = 0;
uint32_t pkt_recv    = 0;
uint32_t pkt_dropped = 0;

void send_hello() {
  HelloMsg h;
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = NODE_ID;
  h.metric = 0;
  h.seq    = hello_seq++;
  memset(h.hmac, 0, HMAC_TAG_LEN);
#if SECURITY_MODE
  sign_hello(&h);
#endif
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[TX] Hello (gateway) seq=%u metric=0 mode=%s\n",
                h.seq, SECURITY_MODE ? "PATCHED" : "BASELINE");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E5b Node B (GATEWAY) mode=%s ===\n",
                SECURITY_MODE ? "PATCHED" : "BASELINE");

  eora_radio_init();
  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  mod   = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, spi);
  radio = new SX1262(mod);
  int st = radio->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                        LORA_SYNC, LORA_POWER, 8, LORA_TCXO);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("[ERR] Radio init: %d\n", st);
    while (true) delay(1000);
  }
  radio->setDio1Action(onDio1);
  radio->startReceive();
  Serial.println("[RADIO] SX1262 ready @ 923 MHz AS923");
}

void loop() {
  uint32_t now = millis();

  if (now - last_hello >= HELLO_PERIOD_MS) {
    last_hello = now;
    send_hello();
  }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t pkt_len = radio->getPacketLength();
    int state = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state == RADIOLIB_ERR_NONE && pkt_len > 0) {
      int len = (int)pkt_len;
      if (buf[0] == MSG_DATA && len >= (int)sizeof(DataMsg)) {
        DataMsg *d = (DataMsg*)buf;
        if (d->dst == NODE_ID) {
          bool valid = true;
#if SECURITY_MODE
          valid = check_data(d);
          if (!valid) {
            pkt_dropped++;
            Serial.printf("[DROP] DATA from 0x%02X HMAC FAILED\n", d->src);
          }
#endif
          if (valid) {
            pkt_recv++;
            Serial.printf("[RX] DATA id=%u from 0x%02X payload=%s | recv=%lu\n",
                          d->pkt_id, d->src, d->payload, pkt_recv);
            AckMsg a;
            a.type   = MSG_ACK;
            a.src    = NODE_ID;
            a.dst    = d->src;
            a.pkt_id = d->pkt_id;
            delay(20);
            radio->transmit((uint8_t*)&a, sizeof(a));
            radio->startReceive();
            Serial.printf("[TX] ACK id=%u → 0x%02X\n", a.pkt_id, a.dst);
          }
        }
      } else if (buf[0] == MSG_HELLO && len >= (int)sizeof(HelloMsg)) {
        HelloMsg *h = (HelloMsg*)buf;
        Serial.printf("[RX] Hello from 0x%02X metric=%u\n", h->src, h->metric);
      }
    }
  }
}
