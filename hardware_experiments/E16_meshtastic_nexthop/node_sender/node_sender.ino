// E16 — Node A: Sender (Meshtastic-format MeshPacket)
//
// Sends MeshPackets with:
//   from = NODE_A, to = NODE_GW (final destination)
//   next_hop = NODE_B (expected relay, set after first delivery)
//
// Demonstrates: after next_hop poisoning by attacker,
//   sender's next_hop is replaced with NODE_C (attacker),
//   who receives packets as an active relay (not silent discard).
//
// Contrast with E5b LoRaMesher: dst=next_hop field collapse means
//   NODE_B discards the packet silently (dst≠self).
//   Here: NODE_B simply is not the next_hop, so does not forward.
//   NODE_C becomes relay and can inspect/drop/forward.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID    0xAA   // sender
#define NODE_GW    0xBB   // gateway (final destination)
#define NODE_RELAY 0x11   // expected relay (honest)
#define NODE_ATK   0xCC   // attacker node

// MeshPacket header — Meshtastic v2.6+ format
// Separate `to` (final dst) and `next_hop` (relay hint) fields
struct MeshPacket {
  uint8_t  from;        // originating node (never changed by relays)
  uint8_t  to;          // final destination
  uint8_t  next_hop;    // relay hint — target of this transmission
  uint16_t pkt_id;
  uint8_t  hop_limit;
  uint8_t  payload[8];  // simulated data
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

uint8_t  current_nexthop = NODE_RELAY;  // updated when poisoned
uint16_t pkt_seq = 0;
uint32_t sent = 0, acked = 0;

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
  oled.drawString(0,  0, "NODE A  SENDER");
  char buf[32];
  snprintf(buf, sizeof(buf), "sent=%u ack=%u", sent, acked);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "next_hop=0x%02X", current_nexthop);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, current_nexthop == NODE_ATK ? "POISONED!" : "normal");
  oled.display();
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E16 Node A (Meshtastic-format Sender) ===");
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
  radio->startReceive();
  Serial.println("[RADIO] ready");
}

// Listen briefly for ROUTE_POISON from attacker updating our next_hop
void check_rx() {
  if (!rxFlag) return;
  rxFlag = false;
  uint8_t buf[32];
  size_t len = radio->getPacketLength();
  int st = radio->readData(buf, sizeof(buf));
  radio->startReceive();
  if (st != RADIOLIB_ERR_NONE || len < 4) return;

  // ROUTE_POISON format: [0xFE, target_node, new_nexthop, 0x00]
  if (buf[0] == 0xFE && buf[1] == NODE_ID) {
    uint8_t poisoned_nh = buf[2];
    Serial.printf("[POISON] next_hop updated: 0x%02X -> 0x%02X\n",
                  current_nexthop, poisoned_nh);
    current_nexthop = poisoned_nh;
    oled_draw();
  }
}

void loop() {
  check_rx();
  static uint32_t last_tx = 0;
  if (millis() - last_tx < 5000) return;
  last_tx = millis();

  MeshPacket pkt;
  pkt.from      = NODE_ID;
  pkt.to        = NODE_GW;       // final destination always gateway
  pkt.next_hop  = current_nexthop; // relay hint — attacker poisons this
  pkt.pkt_id    = pkt_seq++;
  pkt.hop_limit = 3;
  memset(pkt.payload, 0xAA, sizeof(pkt.payload));

  radio->transmit((uint8_t*)&pkt, sizeof(pkt));
  radio->startReceive();
  sent++;

  Serial.printf("[TX] pkt_id=%u to=0x%02X next_hop=0x%02X (seq=%u)\n",
                pkt.pkt_id, pkt.to, pkt.next_hop, sent);
  oled_draw();
}
