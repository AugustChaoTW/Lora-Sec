// E14 — Node A: RSSI Observer
//
// Known position: (0, 0) cm on 2m×2m bench grid.
// Listens for attacker's LocHello, records RSSI, prints RSSI_REPORT line.
// collect_rssi.py reads this line from UART for multilateration.
//
// RSSI_REPORT CSV format:
//   RSSI_REPORT,<node_id_hex>,<seq>,<rssi_dBm>,<pos_x_cm>,<pos_y_cm>
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID     0xBB
#define POS_X_CM     200
#define POS_Y_CM     0

#include "../e14_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);
SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

uint32_t pkts_heard = 0;
float    last_rssi  = 0;
uint16_t last_seq   = 0;
uint32_t last_oled  = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char buf[32];
  snprintf(buf, sizeof(buf), "NODE %02X (%d,%d)cm", NODE_ID, POS_X_CM, POS_Y_CM);
  oled.drawString(0,  0, buf);
  snprintf(buf, sizeof(buf), "RSSI=%.1fdBm", last_rssi);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "seq=%u heard=%lu", last_seq, pkts_heard);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42, "RSSI localize");
  oled.display();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n=== E14 Node 0x%02X RSSI Observer pos=(%d,%d)cm ===\n",
                NODE_ID, POS_X_CM, POS_Y_CM);
  Serial.println("# CSV: RSSI_REPORT,node_id,seq,rssi_dBm,pos_x_cm,pos_y_cm");

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
  Serial.println("[RADIO] ready — listening for attacker LocHello");
}

void loop() {
  if (millis() - last_oled >= 5000) { last_oled = millis(); oled_draw(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[64];
    size_t  pkt_len = radio->getPacketLength();
    float   rssi    = radio->getRSSI();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (buf[0] == MSG_HELLO_LOC && pkt_len >= sizeof(LocHello)) {
      LocHello *h = (LocHello*)buf;
      if (h->src != ID_ATTACKER) return;

      pkts_heard++;
      last_rssi = rssi;
      last_seq  = h->seq;

      // Machine-readable RSSI report for collect_rssi.py
      Serial.printf("RSSI_REPORT,0x%02X,%u,%.2f,%d,%d\n",
                    NODE_ID, h->seq, rssi, POS_X_CM, POS_Y_CM);
      oled_draw();
    }
  }
}
