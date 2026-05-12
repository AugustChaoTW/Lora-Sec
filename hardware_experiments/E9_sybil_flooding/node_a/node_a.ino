// E9 — Node A: SENDER with full routing table (Sybil victim)
//
// Uses a multi-entry routing table (MAX_ROUTES=8) rather than the
// single-best-NH used in E6/E7/E8. This lets us observe routing table
// exhaustion: as Sybil addresses fill slots, we track when the
// legitimate gateway route (dst=0xBB) gets evicted.
//
// Key output to watch:
//   [ROUTE] table full (8/8) — evicting oldest entry
//   [ROUTE] EVICTED dst=0xBB ← gateway route gone → PDR drops to 0%
//   [TX] DATA — no route to 0xBB — dropping
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define SECURITY_MODE   1
#define NODE_ID         0xAA
#define GATEWAY_ID      0xBB
#define DATA_PERIOD_MS   5000
#define MAX_ROUTES       8
#define ROUTE_EXPIRE_MS  300000

#include "../e9_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

struct RouteEntry {
  uint8_t  dst;
  uint8_t  next_hop;
  uint8_t  metric;
  uint32_t last_update_ms;
  bool     valid;
};

RouteEntry rt[MAX_ROUTES];
uint8_t  rt_count   = 0;

uint16_t data_seq   = 0;
uint32_t pkt_sent   = 0;
uint32_t ack_recv   = 0;
uint32_t last_data  = 0;
uint32_t last_oled  = 0;

void rt_print() {
  Serial.printf("[RTABLE] %u/%u entries:\n", rt_count, MAX_ROUTES);
  for (int i = 0; i < MAX_ROUTES; i++) {
    if (rt[i].valid)
      Serial.printf("  [%d] dst=0x%02X nh=0x%02X m=%u age=%lums\n",
                    i, rt[i].dst, rt[i].next_hop, rt[i].metric,
                    millis() - rt[i].last_update_ms);
  }
}

bool rt_has_gateway() {
  for (int i = 0; i < MAX_ROUTES; i++)
    if (rt[i].valid && rt[i].dst == GATEWAY_ID) return true;
  return false;
}

uint8_t rt_nh_to_gateway() {
  for (int i = 0; i < MAX_ROUTES; i++)
    if (rt[i].valid && rt[i].dst == GATEWAY_ID) return rt[i].next_hop;
  return 0xFF;
}

// Update or insert route. If full, evict oldest entry.
void rt_upsert(uint8_t src, uint8_t metric) {
  uint32_t now = millis();
  // Check if dst already in table
  for (int i = 0; i < MAX_ROUTES; i++) {
    if (rt[i].valid && rt[i].dst == src) {
      if (metric < rt[i].metric) {
        rt[i].next_hop = src;
        rt[i].metric   = metric;
      }
      rt[i].last_update_ms = now;
      return;
    }
  }
  // Find empty slot
  for (int i = 0; i < MAX_ROUTES; i++) {
    if (!rt[i].valid) {
      rt[i] = {src, src, metric, now, true};
      rt_count++;
      Serial.printf("[ROUTE] INSERT dst=0x%02X nh=0x%02X m=%u (table=%u/%u)\n",
                    src, src, metric, rt_count, MAX_ROUTES);
      return;
    }
  }
  // Table FULL — evict oldest
  uint32_t oldest_age = 0; int oldest_idx = 0;
  for (int i = 0; i < MAX_ROUTES; i++) {
    uint32_t age = now - rt[i].last_update_ms;
    if (age > oldest_age) { oldest_age = age; oldest_idx = i; }
  }
  bool evicted_gw = (rt[oldest_idx].dst == GATEWAY_ID);
  Serial.printf("[ROUTE] TABLE FULL — evicting dst=0x%02X (age=%lums)%s\n",
                rt[oldest_idx].dst, oldest_age,
                evicted_gw ? " ← GATEWAY EVICTED! PDR→0%" : "");
  if (evicted_gw)
    Serial.println("[SYBIL ATTACK SUCCESS] Gateway route evicted. DoS confirmed.");
  rt[oldest_idx] = {src, src, metric, now, true};
}

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE A  SYBIL VIC");
  char buf[32];
  snprintf(buf, sizeof(buf), "RT=%u/%u GW=%s",
           rt_count, MAX_ROUTES, rt_has_gateway() ? "OK" : "GONE!");
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "PDR=%.0f%%",
           pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, rt_has_gateway() ? "route OK" : "NO GW ROUTE!");
  oled.display();
}

void send_data() {
  uint8_t nh = rt_nh_to_gateway();
  if (nh == 0xFF) {
    Serial.println("[TX] No route to gateway — packet dropped (Sybil DoS active)");
    return;
  }
  DataMsg d = {};
  d.type      = MSG_DATA;
  d.src       = NODE_ID;
  d.dst       = nh;
  d.final_dst = GATEWAY_ID;
  d.pkt_id    = data_seq++;
  snprintf((char*)d.payload, 8, "A%05u", d.pkt_id);
  sign_data(&d);
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  Serial.printf("[TX] DATA id=%u via=0x%02X PDR=%.1f%%\n",
                d.pkt_id, nh, pkt_sent ? (float)ack_recv/pkt_sent*100 : 0);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E9 Node A (Sybil victim, SECURITY_MODE=1) ===");
  Serial.printf("[INFO] Routing table capacity: MAX_ROUTES=%d\n", MAX_ROUTES);
  Serial.println("[INFO] Watch for [SYBIL ATTACK SUCCESS] when GW route is evicted.");
  memset(rt, 0, sizeof(rt));

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
  Serial.println("[RADIO] SX1262 ready @ 923 MHz AS923");
}

void loop() {
  uint32_t now = millis();
  if (now - last_data >= DATA_PERIOD_MS) { last_data = now; send_data(); }
  if (now - last_oled >= 5000)           { last_oled = now; oled_draw(); rt_print(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src == NODE_ID) return;
#if SECURITY_MODE >= 1
      if (!check_hello(h)) {
        Serial.printf("[DROP] Hello 0x%02X HMAC FAILED\n", h->src);
        return;
      }
#endif
      rt_upsert(h->src, h->metric + 1);
      oled_draw();

    } else if (buf[0] == MSG_ACK && pkt_len >= sizeof(AckMsg)) {
      AckMsg *a = (AckMsg*)buf;
      if (a->dst == NODE_ID) {
        ack_recv++;
        Serial.printf("[RX] ACK id=%u PDR=%.1f%%\n",
                      a->pkt_id, (float)ack_recv/pkt_sent*100);
      }
    }
  }
}
