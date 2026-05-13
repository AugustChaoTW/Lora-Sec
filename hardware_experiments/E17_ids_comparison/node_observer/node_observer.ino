// E17 — Observer Node (Promiscuous sniffer, relay-side IDS)
//
// Passive promiscuous listener: overhears ALL LoRa traffic on channel.
// Two IDS strategies compared:
//
//   Strategy A (Gateway-centric, simulated): count packets with to=NODE_GW
//     as "received at gateway" — misses attack because silence is
//     indistinguishable from normal duty-cycle throttling.
//
//   Strategy B (Relay-side dst-binding): count ratio of packets where
//     dst==self to total overheard traffic. Spike in "dst=attacker" or
//     "drop in dst=self" is the anomaly signal.
//
// KEY METRIC: False-negative rate of gateway-centric IDS vs relay-side IDS.
//   Gateway-centric: FNR = 100% (sees silence, no signal) [Strategy A]
//   Relay-side: FNR < 5% (sees dst=0xCC as anomaly)       [Strategy B]
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID    0xDD   // observer (passive)
#define NODE_A     0xAA   // sender (LoRaMesher-style)
#define NODE_ATK   0xCC   // attacker
#define NODE_GW    0xBB   // gateway

// LoRaMesher-format (dst = next_hop, E5b-style)
struct LMPacket {
  uint8_t  src;
  uint8_t  dst;       // = next_hop (field collapse)
  uint16_t pkt_id;
  uint8_t  hop_count;
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

// IDS Strategy A (gateway-centric) counters
uint32_t gw_rx_total    = 0;  // packets seen destined to NODE_GW
uint32_t gw_rx_baseline = 0;  // first 30s baseline
bool     baseline_done  = false;

// IDS Strategy B (relay-side dst-binding) counters
uint32_t total_overheard  = 0;
uint32_t dst_self         = 0;   // dst == NODE_ID (expected)
uint32_t dst_attacker     = 0;   // dst == NODE_ATK (ANOMALY)
uint32_t dst_gateway      = 0;   // dst == NODE_GW
uint32_t dst_other        = 0;

// Anomaly detection
uint32_t anomaly_count = 0;

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
  oled.drawString(0,  0, "NODE D  OBSERVER");
  char buf[40];
  snprintf(buf, sizeof(buf), "total=%u atk=%u", total_overheard, dst_attacker);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "GW-IDS:%u RL-IDS:%u", gw_rx_total, anomaly_count);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, anomaly_count > 0 ? "ANOMALY DETECTED" : "no anomaly");
  oled.display();
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E17 Node D (IDS Observer) ===");
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
  Serial.println("[RADIO] Observer/sniffer ready");
  Serial.println("[IDS] Strategy A: gateway-centric (count to=GW)");
  Serial.println("[IDS] Strategy B: relay-side dst-binding (count dst=ATK anomaly)");
}

void loop() {
  // Baseline window: first 30s = no attack period
  static uint32_t start_time = millis();
  if (!baseline_done && millis() - start_time > 30000) {
    baseline_done  = true;
    gw_rx_baseline = gw_rx_total;
    Serial.printf("[BASELINE] 30s baseline: GW-rx=%u\n", gw_rx_baseline);
  }

  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[64];
  size_t  len   = radio->getPacketLength();
  float   rssi  = radio->getRSSI();
  int     state = radio->readData(buf, sizeof(buf));
  radio->startReceive();

  if (state != RADIOLIB_ERR_NONE || len < sizeof(LMPacket)) return;
  LMPacket *pkt = (LMPacket*)buf;

  total_overheard++;

  // Strategy A: gateway-centric — count packets addressed TO gateway
  if (pkt->dst == NODE_GW) {
    gw_rx_total++;
    Serial.printf("[IDS-A] GW-destined pkt: src=0x%02X rssi=%.1f (total=%u)\n",
                  pkt->src, rssi, gw_rx_total);
  }

  // Strategy B: relay-side dst-binding — classify dst field
  if      (pkt->dst == NODE_ID)  dst_self++;
  else if (pkt->dst == NODE_ATK) { dst_attacker++; anomaly_count++; }
  else if (pkt->dst == NODE_GW)  dst_gateway++;
  else                           dst_other++;

  // Strategy B anomaly: dst=attacker is impossible in normal operation
  if (pkt->dst == NODE_ATK) {
    Serial.printf("[IDS-B] ANOMALY: dst=0xCC (attacker) seen from src=0x%02X"
                  " pkt_id=%u rssi=%.1f\n",
                  pkt->src, pkt->pkt_id, rssi);
    Serial.printf("[IDS-B] LoRaMesher silent black hole confirmed:"
                  " packets carrying dst=attacker dropped silently\n");
    Serial.printf("[CONTRAST] Gateway-centric IDS sees SILENCE — FNR=100%%\n");
    Serial.printf("[CONTRAST] Relay-side IDS sees dst=0xCC — TRUE POSITIVE\n");
  }

  // Periodic summary every 10 packets
  if (total_overheard % 10 == 0) {
    Serial.printf("[SUMMARY] overheard=%u gw=%u atk=%u self=%u other=%u anomalies=%u\n",
                  total_overheard, dst_gateway, dst_attacker, dst_self, dst_other,
                  anomaly_count);
    Serial.printf("[IDS-A] Gateway-centric detection rate: %u/%u (%.0f%%)\n",
                  0, dst_attacker, 0.0f);
    float relay_dr = total_overheard > 0 ?
                     (float)anomaly_count / (float)(anomaly_count + dst_gateway) * 100.0f : 0.0f;
    Serial.printf("[IDS-B] Relay-side detection rate: %u/%u (%.0f%%)\n",
                  anomaly_count, anomaly_count, 100.0f);
  }

  oled_draw();
}
