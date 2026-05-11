// E5c — Node A: DATA SENDER + MetricVersion verifier
// Role  : Sends data to Node B (gateway); verifies incoming Hello freshness.
// Board : Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// SECURITY_MODE:
//   0 = baseline  — accepts any Hello; reboot replay always succeeds
//   1 = HMAC only — rejects forged Hellos; reboot replay still works
//   2 = HMAC + MetricVersion + NVS — reboot replay window eliminated

#define SECURITY_MODE 2   // ← change to 0 or 1 to compare
#define NODE_ID       0xAA
#define GATEWAY_ID    0xBB
#define HELLO_PERIOD_MS  10000
#define DATA_PERIOD_MS    5000

#include "../e5c_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

RouteEntry routes[MAX_ROUTES];
int        n_routes  = 0;
uint16_t   hello_seq = 0;
uint16_t   data_seq  = 0;
uint32_t   last_hello = 0, last_data = 0;
uint32_t   pkt_sent = 0, ack_recv = 0;

// Per-sender MetricVersion storage (SRAM cache; persisted to NVS on change).
struct MvEntry { uint8_t src; uint16_t mv; };
MvEntry mv_cache[MAX_ROUTES];

// OLED status
char oled_line3[24] = "waiting...";
char oled_line4[24] = "";
uint32_t last_oled = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  snprintf(buf, sizeof(buf), "NODE A  MODE:%d", SECURITY_MODE);
  oled.drawString(0, 0, buf);
  uint16_t mv_bb = mv_get(GATEWAY_ID);
  snprintf(buf, sizeof(buf), "MV[BB]=%u  PKT=%lu", mv_bb, pkt_sent);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, oled_line3);
  snprintf(buf, sizeof(buf), "PDR=%.0f%%", pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
  oled.drawString(0, 42, buf);
  oled.display();
}
int     mv_n = 0;

uint16_t mv_get(uint8_t src) {
  for (int i = 0; i < mv_n; i++)
    if (mv_cache[i].src == src) return mv_cache[i].mv;
  return 0;
}

void mv_update(uint8_t src, uint16_t new_mv) {
  for (int i = 0; i < mv_n; i++) {
    if (mv_cache[i].src == src) {
      mv_cache[i].mv = new_mv;
#if SECURITY_MODE >= 2
      nvs_save_mv(src, new_mv);
      Serial.printf("[NVS] Saved mv[0x%02X]=%u\n", src, new_mv);
#endif
      return;
    }
  }
  if (mv_n < MAX_ROUTES) {
    mv_cache[mv_n++] = {src, new_mv};
#if SECURITY_MODE >= 2
    nvs_save_mv(src, new_mv);
    Serial.printf("[NVS] Saved mv[0x%02X]=%u\n", src, new_mv);
#endif
  }
}

void update_route(uint8_t dst, uint8_t next_hop, uint8_t metric,
                  uint16_t mv_new) {
  for (int i = 0; i < n_routes; i++) {
    if (routes[i].dst == dst) {
      if (metric < routes[i].metric) {
        routes[i].next_hop        = next_hop;
        routes[i].metric          = metric;
        routes[i].last_update_ms  = millis();
        routes[i].metric_version  = mv_new;
        mv_update(next_hop, mv_new);
        Serial.printf("[ROUTE] Updated dst=0x%02X via=0x%02X metric=%u mv=%u\n",
                      dst, next_hop, metric, mv_new);
      }
      return;
    }
  }
  if (n_routes < MAX_ROUTES) {
    routes[n_routes++] = {dst, next_hop, metric, millis(), mv_new};
    mv_update(next_hop, mv_new);
    Serial.printf("[ROUTE] Added dst=0x%02X via=0x%02X metric=%u mv=%u\n",
                  dst, next_hop, metric, mv_new);
  }
}

uint8_t best_nexthop(uint8_t dst) {
  uint8_t best_metric = 255, best_nh = dst;
  uint32_t now = millis();
  for (int i = 0; i < n_routes; i++) {
    if (routes[i].dst == dst &&
        routes[i].metric < best_metric &&
        now - routes[i].last_update_ms < ROUTE_EXPIRE_MS) {
      best_metric = routes[i].metric;
      best_nh     = routes[i].next_hop;
    }
  }
  return best_nh;
}

void send_hello() {
  HelloMsg h = {};
  h.type           = MSG_HELLO;
  h.src            = NODE_ID;
  h.dst_gw         = GATEWAY_ID;
  h.metric         = 1;
  h.seq            = hello_seq++;
  h.metric_version = 0;    // Node A is not a gateway; MV=0 for its own Hellos
#if SECURITY_MODE >= 1
  sign_hello(&h);
#endif
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[TX] Hello seq=%u mode=%d\n", h.seq, SECURITY_MODE);
}

void send_data() {
  uint8_t nh = best_nexthop(GATEWAY_ID);
  DataMsg d = {};
  d.type   = MSG_DATA;
  d.src    = NODE_ID;
  d.dst    = nh;
  d.pkt_id = data_seq++;
  snprintf((char*)d.payload, 8, "PKT%04u", d.pkt_id);
#if SECURITY_MODE >= 1
  sign_data(&d);
#endif
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  float pdr = pkt_sent ? (float)ack_recv / pkt_sent * 100.0f : 0;
  Serial.printf("[TX] DATA id=%u via=0x%02X sent=%lu ack=%lu PDR=%.1f%%\n",
                d.pkt_id, nh, pkt_sent, ack_recv, pdr);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E5c Node A  SECURITY_MODE=%d ===\n", SECURITY_MODE);
  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically(); oled.clear();
  oled.setFont(ArialMT_Plain_10); oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0, 0, "NODE A booting...");
  oled.display();

#if SECURITY_MODE >= 2
  // Load persisted MetricVersion counters BEFORE any Hello is processed.
  // This closes the reboot attack window.
  Serial.println("[NVS] Loading persisted MetricVersion counters...");
  uint8_t known_senders[] = {GATEWAY_ID};
  for (uint8_t s : known_senders) {
    uint16_t v = nvs_load_mv(s);
    if (v > 0) {
      mv_cache[mv_n++] = {s, v};
      Serial.printf("[NVS] Loaded mv[0x%02X]=%u (reboot window closed)\n", s, v);
    } else {
      Serial.printf("[NVS] mv[0x%02X]=0 (first boot or no history)\n", s);
    }
  }
#endif

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
  oled_draw();   // clear "booting..." and show initial state
}

void loop() {
  uint32_t now = millis();

  if (now - last_hello >= HELLO_PERIOD_MS) { last_hello = now; send_hello(); }
  if (now - last_data  >= DATA_PERIOD_MS  && n_routes > 0)
    { last_data = now; send_data(); }
  if (now - last_oled  >= 5000) { last_oled = now; oled_draw(); }  // refresh every 5s

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src == NODE_ID) return;   // ignore own echo

#if SECURITY_MODE >= 1
      if (!check_hello(h)) {
        Serial.printf("[DROP] Hello from 0x%02X: HMAC FAILED\n", h->src);
        return;
      }
#endif

#if SECURITY_MODE >= 2
      uint16_t mv_stored = mv_get(h->src);
      if (h->metric_version <= mv_stored) {
        Serial.printf("[DROP] Hello from 0x%02X: MV=%u <= stored=%u (REPLAY REJECTED)\n",
                      h->src, h->metric_version, mv_stored);
        snprintf(oled_line3, sizeof(oled_line3), "DROP MV%u<=stored%u", h->metric_version, mv_stored);
        oled_draw();
        return;
      }
      Serial.printf("[RX] Hello from 0x%02X metric=%u mv=%u [HMAC+MV OK]\n",
                    h->src, h->metric, h->metric_version);
      snprintf(oled_line3, sizeof(oled_line3), "RX OK mv=%u", h->metric_version);
      update_route(GATEWAY_ID, h->src, h->metric + 1, h->metric_version);
#else
      Serial.printf("[RX] Hello from 0x%02X metric=%u\n", h->src, h->metric);
      update_route(GATEWAY_ID, h->src, h->metric + 1, h->metric_version);
#endif

    } else if (buf[0] == MSG_ACK && pkt_len >= sizeof(AckMsg)) {
      AckMsg *a = (AckMsg*)buf;
      if (a->dst == NODE_ID) {
        ack_recv++;
        Serial.printf("[RX] ACK id=%u PDR=%.1f%%\n",
                      a->pkt_id, (float)ack_recv / pkt_sent * 100.0f);
        oled_draw();
      }
    }
  }
}
