// E12 — Node D: FSM GUARDIAN (routing protocol IDS)
//
// Operates in promiscuous mode. Maintains per-node routing FSM state.
// Emits alerts when FSM transitions violate expected protocol behavior.
//
// LoRaMesher DV Routing FSM per observed node:
//   INIT → HELLO_SEEN → ROUTE_ESTABLISHED → DATA_FORWARDING
//
// FSM Violation rules:
//   V1: MetricVersion non-monotonic (seq decreases → replay candidate)
//   V2: Same src, different metric within single convergence window (injection)
//   V3: Rapid Hello re-announcement without ROUTE_EXPIRE timeout (storm indicator)
//   V4: DATA dst == known-attacker address (black hole indicator, cross-layer)
//   V5: src address == known Sybil range (0xD0–0xFF) + valid HMAC (insider Sybil)
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// Reference: extends future-internet-18-03-140 (LoRaWAN OTAA FSM-IDS)
//            to LoRaMesher DV routing layer with multi-hop state tracking.

#define NODE_ID           0xDD
#define MAX_NODES         16
#define CONV_WINDOW_MS    30000    // convergence window for V2 check
#define MIN_HELLO_GAP_MS   5000   // V3: faster than this → suspicious
#define SYBIL_ADDR_START  0xD0
#define SYBIL_ADDR_END    0xFF
#define KNOWN_ATTACKER    0xCC    // V4 cross-layer check

#include <RadioLib.h>
#include <SPI.h>
#include "mbedtls/md.h"
#include <Wire.h>
#include "SSD1306Wire.h"

// Radio pins (EoRa PI)
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

#define HMAC_KEY_LEN  16
#define HMAC_TAG_LEN  16

static const uint8_t NETWORK_PSK[HMAC_KEY_LEN] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

#define MSG_HELLO   0x01
#define MSG_DATA    0x02
#define MSG_ACK     0x03

struct HelloMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst_gw;
  uint8_t  metric;
  uint16_t seq;
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct DataMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst;
  uint8_t  final_dst;
  uint16_t pkt_id;
  uint8_t  payload[8];
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

static bool check_hello_hmac(const HelloMsg *m) {
  uint8_t tmp[sizeof(HelloMsg)];
  memcpy(tmp, m, sizeof(HelloMsg));
  memset(tmp + offsetof(HelloMsg, hmac), 0, HMAC_TAG_LEN);
  uint8_t expected[HMAC_TAG_LEN];
  mbedtls_md_context_t ctx;
  uint8_t full[32];
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, NETWORK_PSK, HMAC_KEY_LEN);
  mbedtls_md_hmac_update(&ctx, tmp, sizeof(HelloMsg) - HMAC_TAG_LEN);
  mbedtls_md_hmac_finish(&ctx, full);
  mbedtls_md_free(&ctx);
  memcpy(expected, full, HMAC_TAG_LEN);
  return memcmp(expected, m->hmac, HMAC_TAG_LEN) == 0;
}

// Per-node FSM state
enum NodeState { STATE_INIT, STATE_HELLO_SEEN, STATE_ROUTE_ESTAB, STATE_DATA_FWD };

struct NodeRecord {
  uint8_t   addr;
  NodeState state;
  uint16_t  last_seq;
  uint8_t   last_metric;
  uint32_t  last_hello_ms;
  uint32_t  conv_window_start;
  uint8_t   metric_in_window;
  bool      metric_conflict;
  uint32_t  alert_count;
  bool      valid;
};

NodeRecord nodes[MAX_NODES];
uint32_t   total_alerts  = 0;
uint32_t   pkts_heard    = 0;
uint32_t   last_oled     = 0;

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

NodeRecord* get_node(uint8_t addr) {
  for (int i = 0; i < MAX_NODES; i++) {
    if (nodes[i].valid && nodes[i].addr == addr) return &nodes[i];
  }
  for (int i = 0; i < MAX_NODES; i++) {
    if (!nodes[i].valid) {
      nodes[i] = {addr, STATE_INIT, 0, 255, 0, 0, 255, false, 0, true};
      return &nodes[i];
    }
  }
  return nullptr;
}

void emit_alert(uint8_t src, const char *violation, const char *detail) {
  total_alerts++;
  Serial.printf("[GUARDIAN] ALERT V:%s src=0x%02X — %s (total=%lu)\n",
                violation, src, detail, total_alerts);
}

void check_hello(const HelloMsg *h) {
  NodeRecord *n = get_node(h->src);
  if (!n) return;

  uint32_t now = millis();
  bool hmac_ok = check_hello_hmac(h);

  // V5: Sybil range address with valid HMAC (insider Sybil)
  if (h->src >= SYBIL_ADDR_START && h->src <= SYBIL_ADDR_END) {
    if (hmac_ok)
      emit_alert(h->src, "V5", "Sybil-range address with valid HMAC (insider)");
    else
      emit_alert(h->src, "V5", "Sybil-range address (unauthenticated)");
  }

  if (!hmac_ok) {
    // Unauthenticated Hello — already caught by HMAC patch but log it
    emit_alert(h->src, "V0", "Hello HMAC FAILED (unauthenticated injection)");
    return;
  }

  // V1: MetricVersion non-monotonic (seq decrease)
  if (n->state >= STATE_HELLO_SEEN && h->seq < n->last_seq) {
    char buf[64];
    snprintf(buf, sizeof(buf), "seq %u → %u (non-monotonic, replay candidate)",
             n->last_seq, h->seq);
    emit_alert(h->src, "V1", buf);
    n->alert_count++;
  }

  // V3: Hello rate anomaly (too fast)
  if (n->last_hello_ms > 0) {
    uint32_t gap = now - n->last_hello_ms;
    if (gap < MIN_HELLO_GAP_MS) {
      char buf[64];
      snprintf(buf, sizeof(buf), "gap=%lums < min=%dms (routing storm / flood)",
               gap, MIN_HELLO_GAP_MS);
      emit_alert(h->src, "V3", buf);
      n->alert_count++;
    }
  }

  // V2: metric conflict within convergence window
  if (now - n->conv_window_start < CONV_WINDOW_MS) {
    if (n->metric_in_window != 255 && n->metric_in_window != h->metric) {
      char buf[64];
      snprintf(buf, sizeof(buf), "metric %u → %u in %lums (injection candidate)",
               n->metric_in_window, h->metric, CONV_WINDOW_MS);
      emit_alert(h->src, "V2", buf);
      n->metric_conflict = true;
      n->alert_count++;
    }
  } else {
    n->conv_window_start = now;
    n->metric_in_window  = h->metric;
    n->metric_conflict   = false;
  }

  // Update state
  n->last_seq      = h->seq;
  n->last_metric   = h->metric;
  n->last_hello_ms = now;
  if (n->state == STATE_INIT) n->state = STATE_HELLO_SEEN;
  if (h->metric == 1 || h->metric == 0) n->state = STATE_ROUTE_ESTAB;
}

void check_data(const DataMsg *d) {
  NodeRecord *n = get_node(d->src);
  if (!n) return;
  if (n->state >= STATE_ROUTE_ESTAB) n->state = STATE_DATA_FWD;

  // V4: data routed to known attacker (Black Hole indicator)
  if (d->dst == KNOWN_ATTACKER) {
    char buf[64];
    snprintf(buf, sizeof(buf), "DATA dst=0x%02X (known attacker — Black Hole route active)",
             KNOWN_ATTACKER);
    emit_alert(d->src, "V4", buf);
  }
}

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE D  GUARDIAN");
  char buf[32];
  snprintf(buf, sizeof(buf), "pkts=%lu alrt=%lu", pkts_heard, total_alerts);
  oled.drawString(0, 14, buf);
  // Count active nodes
  uint8_t active = 0;
  for (int i = 0; i < MAX_NODES; i++) if (nodes[i].valid) active++;
  snprintf(buf, sizeof(buf), "nodes=%u V1-V5 active", active);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, total_alerts > 0 ? "VIOLATIONS DETECTED" : "clean");
  oled.display();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E12 Node D (FSM GUARDIAN — routing IDS) ===");
  Serial.println("[INFO] Promiscuous mode. Monitors V1(replay) V2(injection) V3(storm) V4(blackhole) V5(sybil).");
  memset(nodes, 0, sizeof(nodes));

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
  Serial.println("[RADIO] SX1262 ready — promiscuous FSM monitor");
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
      Serial.printf("[SNIFF] Hello src=0x%02X metric=%u seq=%u rssi=%.0fdBm\n",
                    h->src, h->metric, h->seq, rssi);
      check_hello(h);

    } else if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      Serial.printf("[SNIFF] Data src=0x%02X dst=0x%02X id=%u rssi=%.0fdBm\n",
                    d->src, d->dst, d->pkt_id, rssi);
      check_data(d);
    }
  }
}
