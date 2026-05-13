// E16 — Node B: Gateway (Meshtastic-format receiver)
//
// Receives MeshPackets where to==self (NODE_GW).
// In Meshtastic format, `to` = final destination, so gateway accepts
// packets regardless of who the previous next_hop was.
//
// KEY METRIC: PDR measured at gateway.
// After attack: if attacker in RELAY_HIJACK mode, gateway still receives
//   (via attacker relay) — attacker inspects payload enroute.
// After attack: if attacker in SELECTIVE_DROP mode, gateway receives 0%
//   but sees NO anomaly (contrast: LoRaMesher E5b also 0% but for different reason).
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID  0xBB   // gateway
#define NODE_A   0xAA   // expected sender

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

uint32_t received = 0;
uint32_t via_attacker = 0;  // hop_limit reveals relay path
uint32_t via_direct   = 0;

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
  oled.drawString(0,  0, "NODE B  GATEWAY");
  char buf[32];
  snprintf(buf, sizeof(buf), "recv=%u", received);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "via_atk=%u direct=%u", via_attacker, via_direct);
  oled.drawString(0, 28, buf);
  oled.display();
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E16 Node B (Meshtastic Gateway) ===");
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
  Serial.println("[RADIO] Gateway ready");
}

void loop() {
  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[64];
  size_t  len   = radio->getPacketLength();
  float   rssi  = radio->getRSSI();
  int     state = radio->readData(buf, sizeof(buf));
  radio->startReceive();

  if (state != RADIOLIB_ERR_NONE || len < sizeof(MeshPacket)) return;
  MeshPacket *pkt = (MeshPacket*)buf;

  // Accept only packets addressed to self (Meshtastic: to = final dst)
  if (pkt->to != NODE_ID) return;

  received++;
  // hop_limit=3 means direct; hop_limit=2 means 1 relay hop
  // If via attacker relay, hop_limit will be 2 (attacker decremented it)
  bool via_relay = (pkt->hop_limit < 3);
  if (via_relay) via_attacker++; else via_direct++;

  Serial.printf("[RX] from=0x%02X pkt_id=%u hop_limit=%u rssi=%.1f"
                " via=%s\n",
                pkt->from, pkt->pkt_id, pkt->hop_limit, rssi,
                via_relay ? "RELAY(attacker?)" : "DIRECT");

  // GATEWAY-CENTRIC IDS: gateway cannot distinguish "attacker relay" from "honest relay"
  // This is the key finding: gateway-centric IDS is blind to relay path identity.
  if (via_relay) {
    Serial.printf("[IDS] Gateway-centric check: relay path used — cannot identify if attacker\n");
    Serial.printf("[IDS] BLIND: no anomaly signal at gateway for relay-path attacks\n");
  }

  oled_draw();
}
