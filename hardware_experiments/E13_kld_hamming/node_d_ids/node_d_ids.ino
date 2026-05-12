// E13 — Node D: KLD + Hamming Distance IDS
//
// Adapts ieee2025-lorawan-jamming-ids method to LoRaMesher DV routing:
//   KLD (Kullback-Leibler Divergence): detects Hello rate flooding
//   Hamming Distance: detects replay (identical consecutive MetricVersions)
//
// Feature vector per Hello packet: [inter-Hello interval, seq delta, src]
//
// Phase 1 — Baseline (BASELINE_SECS):
//   Collect inter-Hello interval histogram P[] per src address
//
// Phase 2 — Detection:
//   For each new Hello: compute KLD(Q||P) for that src
//   KLD > KLD_THRESHOLD     → flooding / routing storm
//   HammingDist(seq_t, seq_{t-1}) < HD_THRESHOLD → replay candidate
//   HammingDist == 0        → confirmed replay
//
// KLD approximation (no FPU needed):
//   Uses 8-bin histogram with integer arithmetic scaled by 1000
//   KLD = sum( Q[i] * log(Q[i]/P[i]) ) — implemented with int log2 table
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
// Reference: extends ieee2025-lorawan-jamming-ids (DevNonce KLD → MetricVersion HD)

#define NODE_ID           0xDD
#define MAX_TRACKED_NODES  8
#define BASELINE_SECS     60      // 60s baseline collection
#define KLD_THRESHOLD    200      // ×0.001 = 0.2 nats (integer arithmetic)
#define HD_THRESHOLD       3      // Hamming distance < 3 → replay suspect
#define HIST_BINS          8      // histogram bins for inter-Hello interval
#define BIN_WIDTH_MS    2000      // each bin covers 2s of inter-Hello interval

#include <RadioLib.h>
#include <SPI.h>
#include "mbedtls/md.h"
#include <Wire.h>
#include "SSD1306Wire.h"

#define LORA_NSS    7
#define LORA_DIO1  33
#define LORA_RST    8
#define LORA_BUSY  34
#define LORA_SCK    5
#define LORA_MOSI   6
#define LORA_MISO   3

#define LORA_FREQ   923.0
#define LORA_BW     125.0
#define LORA_SF     9
#define LORA_CR     7
#define LORA_SYNC   0x34
#define LORA_POWER  10
#define LORA_TCXO   1.6f

#define HMAC_TAG_LEN  16
#define MSG_HELLO     0x01

struct HelloMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst_gw;
  uint8_t  metric;
  uint16_t seq;
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

inline void eora_radio_init() {
  pinMode(LORA_NSS,  OUTPUT); digitalWrite(LORA_NSS,  HIGH);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_RST,  OUTPUT);
  digitalWrite(LORA_RST, LOW);  delay(2);
  digitalWrite(LORA_RST, HIGH); delay(20);
  uint32_t t = millis();
  while (digitalRead(LORA_BUSY) && millis() - t < 500) delay(1);
}

// Integer log2 * 1000 lookup table (log2(1)..log2(255))
// For KLD: KLD ≈ sum( Q[i] * (log2(Q[i]) - log2(P[i])) ) * ln(2)
// We approximate with Q[i]/P[i] ratio comparison (no full log needed)

// Per-node tracking
struct NodeTracker {
  uint8_t  addr;
  bool     valid;
  // Baseline histogram (inter-Hello intervals)
  uint32_t baseline_hist[HIST_BINS];
  uint32_t baseline_count;
  bool     baseline_done;
  // Detection histogram (current window)
  uint32_t detect_hist[HIST_BINS];
  uint32_t detect_count;
  // Timing
  uint32_t last_hello_ms;
  uint32_t baseline_start_ms;
  // Sequence tracking for Hamming Distance
  uint16_t prev_seq;
  bool     has_prev_seq;
  // Alert counters
  uint32_t kld_alerts;
  uint32_t hd_alerts;
};

NodeTracker trackers[MAX_TRACKED_NODES];
uint32_t    total_alerts = 0;
uint32_t    pkts_heard   = 0;
uint32_t    last_oled    = 0;

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

NodeTracker* get_tracker(uint8_t addr) {
  for (int i = 0; i < MAX_TRACKED_NODES; i++) {
    if (trackers[i].valid && trackers[i].addr == addr) return &trackers[i];
  }
  for (int i = 0; i < MAX_TRACKED_NODES; i++) {
    if (!trackers[i].valid) {
      memset(&trackers[i], 0, sizeof(NodeTracker));
      trackers[i].addr  = addr;
      trackers[i].valid = true;
      trackers[i].baseline_start_ms = millis();
      Serial.printf("[IDS] New node tracked: 0x%02X\n", addr);
      return &trackers[i];
    }
  }
  return nullptr;
}

uint8_t interval_to_bin(uint32_t interval_ms) {
  uint8_t bin = interval_ms / BIN_WIDTH_MS;
  return bin >= HIST_BINS ? HIST_BINS - 1 : bin;
}

// Hamming distance between two uint16_t values
uint8_t hamming_dist_16(uint16_t a, uint16_t b) {
  uint16_t diff = a ^ b;
  uint8_t count = 0;
  while (diff) { count += diff & 1; diff >>= 1; }
  return count;
}

// KLD approximation using integer ratio comparison
// Returns KLD * 1000 (integer)
// Simplified: compare Q distribution against P using chi-square-like divergence
// KLD(Q||P) = sum_i Q[i] * ln(Q[i]/P[i])
// Approximation: KLD_approx = sum_i |Q[i] - P[i]|^2 / P[i] (chi-squared)
// Scaled by 1000 for integer arithmetic
uint32_t compute_kld_approx(const uint32_t *P_hist, uint32_t P_total,
                             const uint32_t *Q_hist, uint32_t Q_total) {
  if (P_total == 0 || Q_total == 0) return 0;
  uint32_t kld = 0;
  for (int i = 0; i < HIST_BINS; i++) {
    // Normalize: P_norm = P_hist[i]*1000/P_total, Q_norm = Q_hist[i]*1000/Q_total
    uint32_t p = P_hist[i] * 1000 / P_total;
    uint32_t q = Q_hist[i] * 1000 / Q_total;
    if (p == 0) p = 1;  // Laplace smoothing
    // chi-squared contribution: (q-p)^2 / p
    int32_t diff = (int32_t)q - (int32_t)p;
    kld += (uint32_t)(diff * diff) / p;
  }
  return kld;
}

void process_hello(const HelloMsg *h, float rssi) {
  NodeTracker *t = get_tracker(h->src);
  if (!t) return;

  uint32_t now = millis();

  // --- Hamming Distance check ---
  if (t->has_prev_seq) {
    uint8_t hd = hamming_dist_16(h->seq, t->prev_seq);
    if (hd == 0) {
      t->hd_alerts++;
      total_alerts++;
      Serial.printf("[IDS] HD=0 src=0x%02X seq=%u (IDENTICAL — confirmed replay, alerts=%lu)\n",
                    h->src, h->seq, t->hd_alerts);
    } else if (hd < HD_THRESHOLD) {
      t->hd_alerts++;
      total_alerts++;
      Serial.printf("[IDS] HD=%u src=0x%02X seq %u→%u (near-replay suspect, alerts=%lu)\n",
                    hd, h->src, t->prev_seq, h->seq, t->hd_alerts);
    } else {
      Serial.printf("[IDS] HD=%u src=0x%02X seq=%u → normal\n", hd, h->src, h->seq);
    }
  }
  t->prev_seq     = h->seq;
  t->has_prev_seq = true;

  // --- Inter-Hello interval histogram ---
  if (t->last_hello_ms > 0) {
    uint32_t interval = now - t->last_hello_ms;
    uint8_t  bin      = interval_to_bin(interval);

    if (!t->baseline_done) {
      // Baseline phase
      if (now - t->baseline_start_ms < (uint32_t)BASELINE_SECS * 1000) {
        t->baseline_hist[bin]++;
        t->baseline_count++;
        Serial.printf("[IDS] BASELINE src=0x%02X interval=%lums bin=%u (%lu samples)\n",
                      h->src, interval, bin, t->baseline_count);
      } else {
        t->baseline_done = true;
        Serial.printf("[IDS] BASELINE COMPLETE src=0x%02X (%lu samples) — detection active\n",
                      h->src, t->baseline_count);
      }
    } else {
      // Detection phase
      t->detect_hist[bin]++;
      t->detect_count++;

      if (t->detect_count >= 4) {  // need a few samples for KLD
        uint32_t kld = compute_kld_approx(
          t->baseline_hist, t->baseline_count,
          t->detect_hist, t->detect_count);

        Serial.printf("[IDS] KLD=%lu src=0x%02X interval=%lums (threshold=%d)\n",
                      kld, h->src, interval, KLD_THRESHOLD);

        if (kld > KLD_THRESHOLD) {
          t->kld_alerts++;
          total_alerts++;
          Serial.printf("[IDS] KLD ALERT src=0x%02X KLD=%lu > %d — ROUTING FLOOD / STORM (alerts=%lu)\n",
                        h->src, kld, KLD_THRESHOLD, t->kld_alerts);
        }
        // Reset detect window after 8 samples to stay current
        if (t->detect_count >= 8) {
          memset(t->detect_hist, 0, sizeof(t->detect_hist));
          t->detect_count = 0;
        }
      }
    }
  }

  t->last_hello_ms = now;
  (void)rssi;
}

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE D  KLD+HD IDS");
  char buf[32];
  snprintf(buf, sizeof(buf), "heard=%lu alrt=%lu", pkts_heard, total_alerts);
  oled.drawString(0, 14, buf);
  uint8_t baselined = 0;
  for (int i = 0; i < MAX_TRACKED_NODES; i++)
    if (trackers[i].valid && trackers[i].baseline_done) baselined++;
  snprintf(buf, sizeof(buf), "baselined=%u/8", baselined);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, total_alerts > 0 ? "ANOMALY DETECTED" : "learning...");
  oled.display();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E13 Node D (KLD+HD IDS) ===");
  Serial.printf("[INFO] Baseline: %ds  KLD threshold: %d  HD threshold: %d\n",
                BASELINE_SECS, KLD_THRESHOLD, HD_THRESHOLD);
  Serial.println("[INFO] Adapts ieee2025-lorawan-jamming-ids to LoRaMesher DV routing layer.");
  memset(trackers, 0, sizeof(trackers));

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
  Serial.println("[RADIO] SX1262 ready — promiscuous IDS mode");
}

void loop() {
  if (millis() - last_oled >= 5000) { last_oled = millis(); oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    float   rssi    = radio->getRSSI();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    pkts_heard++;
    if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      process_hello(h, rssi);
    }
  }
}
