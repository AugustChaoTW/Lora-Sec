// E5c — Node B: GATEWAY
// Role  : Receives data from Node A and sends ACK.
//         Broadcasts Hello with monotonically increasing MetricVersion.
// Board : Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc
//
// SECURITY_MODE must match Node A.

#define SECURITY_MODE 2
#define NODE_ID       0xBB
#define HELLO_PERIOD_MS  10000

#include "../e5c_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

void oled_draw(uint16_t mv, uint32_t pkt_recv) {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0, 0,  "NODE B  GATEWAY");
  char buf[32];
  snprintf(buf, sizeof(buf), "MV: %u", mv);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "RX PKT: %lu", pkt_recv);
  oled.drawString(0, 28, buf);
  snprintf(buf, sizeof(buf), "MODE: %d", SECURITY_MODE);
  oled.drawString(0, 42, buf);
  oled.display();
}

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t hello_seq      = 0;
uint16_t metric_version = 0;   // increments each Hello broadcast
uint32_t last_hello     = 0;
uint32_t pkt_recv       = 0;

// Node B's MetricVersion is NVS-persisted so replays also fail against
// a rebooted Node A that reloads its stored MV.
// On first boot mv = 0; each broadcast increments and saves.
void load_mv_from_nvs() {
  metric_version = nvs_load_mv(NODE_ID);
  Serial.printf("[NVS] Loaded my MetricVersion=%u\n", metric_version);
}

void send_hello() {
  metric_version++;          // monotonically increasing
  HelloMsg h = {};
  h.type           = MSG_HELLO;
  h.src            = NODE_ID;
  h.dst_gw         = NODE_ID;   // I am the gateway
  h.metric         = 0;
  h.seq            = hello_seq++;
  h.metric_version = metric_version;
#if SECURITY_MODE >= 1
  sign_hello(&h);
#endif
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
#if SECURITY_MODE >= 2
  nvs_save_mv(NODE_ID, metric_version);  // persist own MV
#endif
  Serial.printf("[TX] Hello mv=%u seq=%u\n", metric_version, h.seq);
}

void send_ack(uint8_t dst, uint16_t pkt_id) {
  AckMsg a = {};
  a.type   = MSG_ACK;
  a.src    = NODE_ID;
  a.dst    = dst;
  a.pkt_id = pkt_id;
  radio->transmit((uint8_t*)&a, sizeof(a));
  radio->startReceive();
  Serial.printf("[TX] ACK id=%u → 0x%02X\n", pkt_id, dst);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E5c Node B (GATEWAY)  SECURITY_MODE=%d ===\n", SECURITY_MODE);
  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically();
  oled.clear(); oled.setFont(ArialMT_Plain_10);
  oled.drawString(0, 0, "NODE B booting...");
  oled.display();

#if SECURITY_MODE >= 2
  load_mv_from_nvs();
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
  oled_draw(metric_version, pkt_recv);   // clear "booting..."
}

uint32_t last_oled_b = 0;

void loop() {
  uint32_t now = millis();

  if (now - last_hello  >= HELLO_PERIOD_MS) { last_hello = now; send_hello(); }
  if (now - last_oled_b >= 5000) { last_oled_b = now; oled_draw(metric_version, pkt_recv); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      if (d->dst != NODE_ID) return;   // not addressed to me

#if SECURITY_MODE >= 1
      if (!check_data(d)) {
        Serial.printf("[DROP] DATA from 0x%02X: HMAC FAILED\n", d->src);
        return;
      }
#endif
      pkt_recv++;
      Serial.printf("[RX] DATA id=%u from 0x%02X payload=%.8s (total=%lu)\n",
                    d->pkt_id, d->src, d->payload, pkt_recv);
      oled_draw(metric_version, pkt_recv);
      send_ack(d->src, d->pkt_id);
    }
  }
}
