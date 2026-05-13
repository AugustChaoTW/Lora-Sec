// E18 — Patch Asymmetry: HMAC-authenticated Meshtastic sender
//
// Shows patch INEFFECTIVENESS for Meshtastic:
//   Even with HMAC on NodeInfo/RouteUpdate control messages,
//   the next_hop field in MeshPacket header is NOT authenticated.
//   Attacker can intercept and rewrite next_hop in-flight,
//   OR use ROUTE_POISON to update sender's next_hop table.
//
// The root cause: HMAC protects the control plane (NodeInfo),
// but the attack vector is the DATA PLANE (next_hop field in packet header).
// These are different layers — HMAC on control plane is insufficient.
//
// ASYMMETRY: Same HMAC patch closes LoRaMesher attack (control-plane only)
//            but does NOT close Meshtastic attack (data-plane next_hop).
//
// Correct Meshtastic patch: bind next_hop in AEAD AAD, or sign MeshPacket header.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID    0xAA
#define NODE_GW    0xBB
#define NODE_RELAY 0x11   // expected honest relay
#define NODE_ATK   0xCC

// MeshPacket — Meshtastic v2.6+ format
// next_hop field is NOT authenticated — this is the vulnerability
struct MeshPacket {
  uint8_t  from;
  uint8_t  to;
  uint8_t  next_hop;    // UNPROTECTED — attacker can poison this
  uint16_t pkt_id;
  uint8_t  hop_limit;
  uint8_t  payload[8];  // would be AES-CTR encrypted in real Meshtastic
} __attribute__((packed));

// Authenticated NodeInfo (HMAC on control message — THIS PART IS PATCHED)
struct AuthNodeInfo {
  uint8_t  src;
  uint8_t  type;   // 0xE0 = NodeInfo
  uint16_t seq;
  uint8_t  hmac[4];
} __attribute__((packed));

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "SSD1306Wire.h"

static const uint8_t PSK[4] = {0xDE, 0xAD, 0xBE, 0xEF};

void compute_hmac(const uint8_t *data, size_t len, uint8_t out[4]) {
  uint8_t acc[4] = {PSK[0], PSK[1], PSK[2], PSK[3]};
  for (size_t i = 0; i < len; i++) acc[i % 4] ^= data[i];
  memcpy(out, acc, 4);
}

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

uint8_t  current_nexthop = NODE_RELAY;
uint16_t pkt_seq  = 0;
uint16_t info_seq = 0;
uint32_t sent     = 0;
bool     poisoned = false;

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
  oled.drawString(0,  0, "NODE A  MESH+HMAC");
  char buf[32];
  snprintf(buf, sizeof(buf), "sent=%u nh=0x%02X", sent, current_nexthop);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, "PATCH: HMAC on NodeInfo");
  oled.drawString(0, 42, poisoned ? "INEFFECTIVE: POISONED!" : "HMAC ok, nh ok");
  oled.display();
}

void send_auth_nodeinfo() {
  AuthNodeInfo ni;
  ni.src  = NODE_ID;
  ni.type = 0xE0;
  ni.seq  = info_seq++;
  compute_hmac((uint8_t*)&ni, offsetof(AuthNodeInfo, hmac), ni.hmac);
  radio->transmit((uint8_t*)&ni, sizeof(ni));
  radio->startReceive();
}

void check_rx() {
  if (!rxFlag) return;
  rxFlag = false;
  uint8_t buf[32];
  size_t len = radio->getPacketLength();
  int st = radio->readData(buf, sizeof(buf));
  radio->startReceive();
  if (st != RADIOLIB_ERR_NONE || len < 4) return;

  // ROUTE_POISON: [0xFE, target, new_nexthop, 0x00]
  // NOTE: This is NOT a NodeInfo — NOT covered by HMAC on NodeInfo!
  // Attacker sends raw poison, sender accepts it because ONLY NodeInfo is HMAC-protected.
  if (buf[0] == 0xFE && buf[1] == NODE_ID) {
    uint8_t new_nh = buf[2];
    Serial.printf("[POISON] Meshtastic next_hop poisoned: 0x%02X -> 0x%02X\n",
                  current_nexthop, new_nh);
    Serial.printf("[PATCH-FAIL] HMAC on NodeInfo does NOT protect next_hop field!\n");
    Serial.printf("[ROOT-CAUSE] Attack vector is MeshPacket.next_hop (data plane),\n");
    Serial.printf("             not NodeInfo (control plane). Different layer.\n");
    Serial.printf("[CORRECT-FIX] Bind next_hop in AEAD AAD or sign MeshPacket header.\n");
    current_nexthop = new_nh;
    poisoned = (new_nh == NODE_ATK);
    oled_draw();
  }
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E18 Node A (Meshtastic + HMAC NodeInfo patch) ===");
  Serial.println("[PATCH] HMAC on NodeInfo — but next_hop field UNPROTECTED");
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
  for (int i = 0; i < 3; i++) { send_auth_nodeinfo(); delay(200); }
  radio->startReceive();
  Serial.println("[RADIO] ready — NodeInfo HMAC-protected, next_hop VULNERABLE");
}

void loop() {
  check_rx();

  static uint32_t last_info = 0;
  if (millis() - last_info > 10000) {
    last_info = millis();
    send_auth_nodeinfo();
  }

  static uint32_t last_tx = 0;
  if (millis() - last_tx < 5000) return;
  last_tx = millis();

  MeshPacket pkt;
  pkt.from      = NODE_ID;
  pkt.to        = NODE_GW;
  pkt.next_hop  = current_nexthop;  // POISONABLE despite HMAC on NodeInfo
  pkt.pkt_id    = pkt_seq++;
  pkt.hop_limit = 3;
  memset(pkt.payload, 0xAA, sizeof(pkt.payload));

  radio->transmit((uint8_t*)&pkt, sizeof(pkt));
  radio->startReceive();
  sent++;

  Serial.printf("[TX] pkt_id=%u to=0x%02X next_hop=0x%02X poisoned=%s\n",
                pkt.pkt_id, pkt.to, pkt.next_hop,
                (pkt.next_hop == NODE_ATK) ? "YES (HMAC insufficient!)" : "no");
  Serial.printf("[ASYMMETRY] LoRaMesher+HMAC: BLOCKS attack | Meshtastic+HMAC: FAILS\n");
  oled_draw();
}
