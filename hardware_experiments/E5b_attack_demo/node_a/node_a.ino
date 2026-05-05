// E5b — Node A: DATA SENDER
// Role   : Source node — sends data packets to Node B (gateway) via routing table
// Port   : /dev/cu.usbmodem1201  MAC: 1C:DB:D4:85:6E:58
// Board  : Ebyte EoRa PI (ESP32-S3FH4R2 + E22-900MM22S / SX1262)
// FQBN   : esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// Change SECURITY_MODE below to switch baseline ↔ patched:
//   0 = baseline (no HMAC) — attacker can poison routing table
//   1 = patched  (HMAC)    — attacker's forged Hello is rejected

#define SECURITY_MODE 0   // ← change to 1 for patched run
#define NODE_ID       0xAA
#define GATEWAY_ID    0xBB
#define HELLO_PERIOD_MS   10000
#define DATA_PERIOD_MS    5000

#include "../e5b_common.h"

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

RouteEntry routes[MAX_ROUTES];
int        n_routes  = 0;
uint16_t   hello_seq = 0;
uint16_t   data_seq  = 0;
uint32_t   last_hello = 0;
uint32_t   last_data  = 0;
uint32_t   pkt_sent  = 0;
uint32_t   ack_recv  = 0;

void update_route(uint8_t dst, uint8_t next_hop, uint8_t metric) {
  for (int i = 0; i < n_routes; i++) {
    if (routes[i].dst == dst) {
      if (metric < routes[i].metric) {
        routes[i].next_hop       = next_hop;
        routes[i].metric         = metric;
        routes[i].last_update_ms = millis();
        Serial.printf("[ROUTE] Updated dst=0x%02X via=0x%02X metric=%u\n",
                      dst, next_hop, metric);
      }
      return;
    }
  }
  if (n_routes < MAX_ROUTES) {
    routes[n_routes++] = {dst, next_hop, metric, millis()};
    Serial.printf("[ROUTE] Added dst=0x%02X via=0x%02X metric=%u\n",
                  dst, next_hop, metric);
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
  HelloMsg h;
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = 1;
  h.seq    = hello_seq++;
  memset(h.hmac, 0, HMAC_TAG_LEN);
#if SECURITY_MODE
  sign_hello(&h);
#endif
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[TX] Hello seq=%u metric=1 mode=%s\n",
                h.seq, SECURITY_MODE ? "PATCHED" : "BASELINE");
}

void send_data() {
  uint8_t nh = best_nexthop(GATEWAY_ID);
  DataMsg d;
  d.type   = MSG_DATA;
  d.src    = NODE_ID;
  d.dst    = nh;         // next-hop routing: gateway only accepts if dst==self
  d.pkt_id = data_seq++;
  snprintf((char*)d.payload, 8, "PKT%04u", d.pkt_id);
  memset(d.hmac, 0, HMAC_TAG_LEN);
#if SECURITY_MODE
  sign_data(&d);
#endif
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  float pdr = (float)ack_recv / pkt_sent * 100.0f;
  Serial.printf("[TX] DATA id=%u via=0x%02X | sent=%lu ack=%lu PDR=%.1f%%\n",
                d.pkt_id, nh, pkt_sent, ack_recv, pdr);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E5b Node A (SENDER) mode=%s ===\n",
                SECURITY_MODE ? "PATCHED" : "BASELINE");

  eora_radio_init();
  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  mod   = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, spi);
  radio = new SX1262(mod);
  int st = radio->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                        LORA_SYNC, LORA_POWER, 8, LORA_TCXO);
  if (st != RADIOLIB_ERR_NONE) {
    Serial.printf("[ERR] Radio init: %d\n", st);
    while (true) delay(1000);
  }
  radio->setDio1Action(onDio1);
  radio->startReceive();
  Serial.println("[RADIO] SX1262 ready @ 923 MHz AS923");
  Serial.println("[INFO] Listening (interrupt-based)...");
}

void loop() {
  uint32_t now = millis();

  if (now - last_hello >= HELLO_PERIOD_MS) {
    last_hello = now;
    send_hello();
  }

  if (now - last_data >= DATA_PERIOD_MS && n_routes > 0) {
    last_data = now;
    send_data();
  }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t pkt_len = radio->getPacketLength();
    int state = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state == RADIOLIB_ERR_NONE && pkt_len > 0) {
      int len = (int)pkt_len;
      if (buf[0] == MSG_HELLO && len >= (int)sizeof(HelloMsg)) {
        HelloMsg *h = (HelloMsg*)buf;
        bool valid = true;
#if SECURITY_MODE
        valid = check_hello(h);
        if (!valid)
          Serial.printf("[DROP] Hello from 0x%02X HMAC FAILED\n", h->src);
#endif
        if (valid && h->src != NODE_ID) {
          Serial.printf("[RX] Hello from 0x%02X metric=%u%s\n",
                        h->src, h->metric,
                        SECURITY_MODE ? " [HMAC OK]" : "");
          update_route(GATEWAY_ID, h->src, h->metric + 1);
        }
      } else if (buf[0] == MSG_ACK && len >= (int)sizeof(AckMsg)) {
        AckMsg *a = (AckMsg*)buf;
        if (a->dst == NODE_ID) {
          ack_recv++;
          float pdr = (float)ack_recv / pkt_sent * 100.0f;
          Serial.printf("[RX] ACK id=%u PDR=%.1f%%\n", a->pkt_id, pdr);
        }
      }
    }
  }
}
