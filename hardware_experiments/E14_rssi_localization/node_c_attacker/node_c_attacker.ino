// E14 — Attacker Node (0xFF): LocHello broadcaster
//
// Broadcasts LocHello every 2s. Observer nodes record RSSI.
// Place at unknown position; multilateration estimates (x,y).
//
// For calibration runs: place at known positions (e.g., (50,50), (150,100))
// and verify that multilateration output matches. Calibration establishes
// PL_RSSI_REF and PL_N for this specific hardware + environment.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID         0xFF
#define BEACON_PERIOD_MS  2000

#include "../e14_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);
SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

uint16_t seq      = 0;
uint32_t last_tx  = 0;
uint32_t last_oled = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "ATTACKER 0xFF");
  oled.drawString(0, 14, "LocHello beacon");
  char buf[32];
  snprintf(buf, sizeof(buf), "seq=%u every %ds", seq, BEACON_PERIOD_MS/1000);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, "RSSI localize target");
  oled.display();
}

void send_beacon() {
  LocHello h = {};
  h.type        = MSG_HELLO_LOC;
  h.src         = NODE_ID;
  h.seq         = seq++;
  h.tx_power_dbm = LORA_POWER;
  radio->transmit((uint8_t*)&h, sizeof(h));
  radio->startReceive();
  Serial.printf("[TX] LocHello seq=%u tx=%ddBm\n", h.seq, LORA_POWER);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E14 Attacker 0xFF (LocHello beacon) ===");
  Serial.println("[INFO] Place at unknown position. Observer nodes record RSSI.");
  Serial.println("[INFO] collect_rssi.py computes multilateration estimate.");

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
  radio->startReceive();
  Serial.println("[RADIO] ready");

  send_beacon(); last_tx = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - last_tx   >= BEACON_PERIOD_MS) { last_tx   = now; send_beacon(); }
  if (now - last_oled >= 5000)             { last_oled = now; oled_draw(); }
}
