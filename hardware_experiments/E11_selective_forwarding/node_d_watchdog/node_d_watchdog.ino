// E11 — Node D: WATCHDOG (promiscuous packet correlation monitor)
//
// Listens to all LoRa packets (no dst filtering — all nodes share the
// same SX1262 sync word and frequency, so every board hears every TX).
//
// Detection logic:
//   1. When node R1(0x11) forwards DATA pkt_id=N from src=0xAA:
//      log "seen_at_R1[N] = true"
//   2. Within WATCHDOG_WINDOW_MS, check if ROGUE(0x22) re-broadcasts
//      a DATA with same pkt_id from same original src
//   3. If pkt_id seen at R1 but NOT re-broadcast by ROGUE within window:
//      increment suspect_drops[0x22]
//   4. If suspect_drops[0x22] >= ALERT_THRESHOLD: emit ALERT
//
// Limitation: this requires promiscuous overhearing, which is only
// reliable in dense/small topologies. In outdoor deployments, node D
// may not hear all hops. This is stated explicitly in the paper.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID           0xDD
#define WATCHDOG_WINDOW_MS  3000    // time to wait for re-broadcast after R1 fwd
#define ALERT_THRESHOLD      3     // drops before alert fires

#include "../e11_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

// Track packets observed at each hop
struct PktRecord {
  uint16_t pkt_id;
  uint8_t  src;
  uint32_t seen_at_r1_ms;
  bool     relayed_by_rogue;
};

#define PKT_TRACK_SIZE 32
PktRecord pkt_track[PKT_TRACK_SIZE];
uint8_t   pkt_track_idx = 0;

uint32_t suspect_drops  = 0;
uint32_t confirmed_fwd  = 0;
uint32_t total_heard    = 0;
bool     alert_fired    = false;
uint32_t last_oled      = 0;
uint32_t last_audit     = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE D  WATCHDOG");
  char buf[32];
  snprintf(buf, sizeof(buf), "heard=%lu fwd=%lu", total_heard, confirmed_fwd);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "suspect_drop=%lu", suspect_drops);
  oled.drawString(0, 28, buf);
  if (alert_fired)
    oled.drawString(0, 42, "!!! ROGUE DETECTED !!!");
  else
    oled.drawString(0, 42, "monitoring...");
  oled.display();
}

PktRecord* find_or_create(uint16_t pkt_id, uint8_t src) {
  for (int i = 0; i < PKT_TRACK_SIZE; i++) {
    if (pkt_track[i].pkt_id == pkt_id && pkt_track[i].src == src)
      return &pkt_track[i];
  }
  PktRecord *r = &pkt_track[pkt_track_idx % PKT_TRACK_SIZE];
  pkt_track_idx++;
  *r = {pkt_id, src, 0, false};
  return r;
}

void audit_expired_records() {
  uint32_t now = millis();
  for (int i = 0; i < PKT_TRACK_SIZE; i++) {
    PktRecord *r = &pkt_track[i];
    if (r->seen_at_r1_ms == 0) continue;
    if (now - r->seen_at_r1_ms < WATCHDOG_WINDOW_MS) continue;
    if (!r->relayed_by_rogue) {
      // R1 forwarded it; ROGUE did not re-broadcast within window
      suspect_drops++;
      Serial.printf("[WATCHDOG] SUSPECT DROP: src=0x%02X pkt_id=%u — R1 fwd'd, ROGUE silent (total=%lu)\n",
                    r->src, r->pkt_id, suspect_drops);
      if (suspect_drops >= ALERT_THRESHOLD && !alert_fired) {
        alert_fired = true;
        Serial.printf("[WATCHDOG] !!! ALERT: 0x%02X suspected ROGUE RELAY (drops=%lu >= threshold=%d)\n",
                      ID_ROGUE, suspect_drops, ALERT_THRESHOLD);
        Serial.println("[WATCHDOG] Detection method: packet correlation (R1 heard, ROGUE silent)");
        oled_draw();
      }
    } else {
      confirmed_fwd++;
    }
    r->seen_at_r1_ms = 0;  // reset record
    r->relayed_by_rogue = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E11 Node D (WATCHDOG — promiscuous packet correlation) ===");
  Serial.printf("[INFO] Window: %dms  Alert threshold: %d drops\n",
                WATCHDOG_WINDOW_MS, ALERT_THRESHOLD);
  Serial.println("[INFO] Listens for R1 fwd'd packets missing re-broadcast from ROGUE.");
  memset(pkt_track, 0, sizeof(pkt_track));

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
  Serial.println("[RADIO] SX1262 ready — promiscuous mode (no dst filter)");
}

void loop() {
  uint32_t now = millis();
  if (now - last_oled  >= 5000) { last_oled  = now; oled_draw(); }
  if (now - last_audit >= 500)  { last_audit = now; audit_expired_records(); }

  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();
    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    total_heard++;

    if (buf[0] == MSG_DATA && pkt_len >= sizeof(DataMsg)) {
      DataMsg *d = (DataMsg*)buf;
      // No dst filter — hear all DATA packets
      PktRecord *r = find_or_create(d->pkt_id, d->src);

      if (d->dst == ID_ROGUE) {
        // R1 → ROGUE: this is the forwarded packet R1 sent to ROGUE
        r->seen_at_r1_ms = millis();
        Serial.printf("[WATCH] Pkt src=0x%02X id=%u received by ROGUE — expecting re-broadcast within %dms\n",
                      d->src, d->pkt_id, WATCHDOG_WINDOW_MS);
      }
      else if (d->src == 0x22 || d->dst != ID_ROGUE) {
        // A DATA packet re-broadcast by ROGUE onward (dst=0x33 or similar)
        if (r->seen_at_r1_ms > 0 && !r->relayed_by_rogue) {
          r->relayed_by_rogue = true;
          Serial.printf("[WATCH] Pkt src=0x%02X id=%u was relayed by ROGUE — OK\n",
                        d->src, d->pkt_id);
        }
      }
    }
  }
}
