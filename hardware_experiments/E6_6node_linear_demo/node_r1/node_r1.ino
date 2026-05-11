// E6 — Relay Node (R1 / R2 / R3)
// Role: Re-broadcasts received Hellos with metric+1 (multi-hop advertisement).
//       Also acts as a sensor node — sends its own data to gateway.
//       Forwards Data packets from upstream nodes toward gateway.
//
// Configure by changing NODE_ID and UPSTREAM_ACCEPT[] below.
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// SECURITY_MODE:
//   0 = no auth (baseline)
//   1 = HMAC-SHA256 (patched)
//
// Topology:  A(0xAA) → R1(0x11) → R2(0x22) → R3(0x33) → B(0xBB)

#define SECURITY_MODE  1    // ← change to 0 for baseline run
#define NODE_ID        0x11 // ← 0x11=R1, 0x22=R2, 0x33=R3
#define GATEWAY_ID     0xBB
#define DATA_PERIOD_MS  7000   // offset from Node A's 5s to reduce collisions
#define RELAY_DELAY_MS   500   // wait before re-broadcasting received Hello

#include "../e6_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint8_t  best_nh     = GATEWAY_ID;
uint8_t  best_metric = 255;
uint32_t last_update = 0;

uint16_t data_seq    = 0;
uint16_t relay_seq   = 0;
uint32_t pkt_sent    = 0;
uint32_t ack_recv    = 0;
uint32_t relay_count = 0;
uint32_t drop_count  = 0;
uint32_t last_data   = 0;
uint32_t last_oled   = 0;
char     status[24]  = "waiting...";

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  snprintf(buf, sizeof(buf), "RELAY %02X  MODE:%d", NODE_ID, SECURITY_MODE);
  oled.drawString(0,  0, buf);
  snprintf(buf, sizeof(buf), "NH=0x%02X m=%u", best_nh, best_metric);
  oled.drawString(0, 14, buf);
  oled.drawString(0, 28, status);
  snprintf(buf, sizeof(buf), "FWD=%lu DROP=%lu", relay_count, drop_count);
  oled.drawString(0, 42, buf);
  oled.display();
}

void relay_hello(const HelloMsg *src_hello) {
  // Re-broadcast the Hello with our own src and metric+1
  // This advertises "I can reach the gateway with metric = src_hello->metric + 1"
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;           // we are the advertiser
  h.dst_gw = GATEWAY_ID;
  h.metric = src_hello->metric + 1;
  h.seq    = relay_seq++;
#if SECURITY_MODE >= 1
  sign_hello(&h);               // sign with our key
#endif
  delay(RELAY_DELAY_MS);        // small backoff to reduce collision
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  relay_count++;
  Serial.printf("[RELAY] Re-broadcast Hello metric=%u (learned from 0x%02X metric=%u)\n",
                h.metric, src_hello->src, src_hello->metric);
}

void send_data() {
  DataMsg d = {};
  d.type      = MSG_DATA;
  d.src       = NODE_ID;
  d.dst       = best_nh;
  d.final_dst = GATEWAY_ID;
  d.pkt_id    = data_seq++;
  snprintf((char*)d.payload, 8, "%02X%04u", NODE_ID, d.pkt_id);
#if SECURITY_MODE >= 1
  sign_data(&d);
#endif
  radio->transmit((uint8_t*)&d, sizeof(d));
  radio->startReceive();
  pkt_sent++;
  Serial.printf("[TX] DATA id=%u via=0x%02X\n", d.pkt_id, best_nh);
}

void forward_data(const DataMsg *incoming) {
  // Forward data from upstream node toward gateway
  DataMsg fwd = *incoming;
  fwd.dst = best_nh;            // update next hop to our best NH
  // Re-sign if patched (we re-sign as forwarder is honest node)
#if SECURITY_MODE >= 1
  sign_data(&fwd);
#endif
  radio->transmit((uint8_t*)&fwd, sizeof(fwd));
  radio->startReceive();
  Serial.printf("[FWD] DATA id=%u src=0x%02X → 0x%02X\n",
                fwd.pkt_id, fwd.src, fwd.dst);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E6 Relay 0x%02X  SECURITY_MODE=%d ===\n", NODE_ID, SECURITY_MODE);
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
  if (now - last_data >= DATA_PERIOD_MS && best_metric < 255)
    { last_data = now; send_data(); }
  if (now - last_oled >= 5000)
    { last_oled = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src == NODE_ID) return;   // ignore own relay echo

#if SECURITY_MODE >= 1
      if (!check_hello(h)) {
        drop_count++;
        Serial.printf("[DROP] Hello from 0x%02X: HMAC FAILED\n", h->src);
        snprintf(status, sizeof(status), "DROP 0x%02X HMAC", h->src);
        oled_draw();
        return;
      }
#endif
      // Update own route to gateway
      uint8_t cand_metric = h->metric + 1;
      if (cand_metric < best_metric || now - last_update > 30000) {
        best_nh     = h->src;
        best_metric = cand_metric;
        last_update = now;
        Serial.printf("[ROUTE] NH=0x%02X metric=%u\n", best_nh, best_metric);
      }
      // Re-broadcast Hello to advertise ourselves as relay
      relay_hello(h);
      snprintf(status, sizeof(status), "RELAY m=%u ok", h->metric + 1);
      oled_draw();

    } else if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      if (d->src == NODE_ID) return;

      if (d->dst == NODE_ID) {
        // Packet addressed to us as next hop — forward toward gateway
#if SECURITY_MODE >= 1
        if (!check_data(d)) {
          drop_count++;
          Serial.printf("[DROP] DATA from 0x%02X: HMAC FAILED\n", d->src);
          return;
        }
#endif
        if (d->final_dst == GATEWAY_ID && best_metric < 255) {
          forward_data(d);
        }
      }

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
