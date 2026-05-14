// E20 — Node Relay: Configurable adversarial relay for FC-HMAC attack testing
//
// Modes (set RELAY_MODE at compile time):
//   MODE_HONEST      — forward all fragments unchanged
//   MODE_DROP        — drop DROP_RATE_PCT% of fragments randomly
//   MODE_BITFLIP     — flip 1 byte in payload of every BITFLIP_INTERVAL-th frag
//   MODE_REPLAY      — replay last seen frag_id=0 after image completes
//
// Metrics reported:
//   - fragments received, forwarded, dropped, flipped, replayed
//   - FC-HMAC chain break expected vs actual at receiver
//
// Hardware: Ebyte EoRa PI (ESP32-S3FH4R2)

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "SSD1306Wire.h"

#define LORA_NSS    7
#define LORA_DIO1  33
#define LORA_RST    8
#define LORA_BUSY  34
#define LORA_SCK    5
#define LORA_MOSI   6
#define LORA_MISO   3
#define LORA_FREQ  922.0
#define LORA_BW    125.0
#define LORA_SF      9
#define LORA_CR      7
#define LORA_SYNC  0x34
#define LORA_POWER   5
#define LORA_TCXO  1.6f

#define E19_MAGIC_0   0xE1
#define E19_MAGIC_1   0xAA   // receive magic (from sender)
#define FWD_MAGIC_1   0x9A   // forward magic (to receiver)
#define FRAG_HEADER_SIZE  7
#define HMAC_TRUNC_SIZE   8

// --- Relay mode configuration ---
#define MODE_HONEST   0
#define MODE_DROP     1
#define MODE_BITFLIP  2
#define MODE_REPLAY   3

#define RELAY_MODE      MODE_REPLAY   // change per experiment
#define DROP_RATE_PCT   20             // % fragments to drop (MODE_DROP)
#define BITFLIP_INTERVAL 3          // flip every N-th fragment (MODE_BITFLIP)
#define BITFLIP_OFFSET   4          // byte offset within payload to flip

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;
volatile bool rxFlag  = false;
volatile bool txDone  = false;

void IRAM_ATTR onDio1() { rxFlag = true; }
void IRAM_ATTR onTxDone_() { txDone = true; }

uint32_t frags_rx        = 0;
uint32_t frags_forwarded = 0;
uint32_t frags_dropped   = 0;
uint32_t frags_flipped   = 0;
uint32_t frags_replayed  = 0;
uint32_t img_count       = 0;

uint8_t  replay_buf[255];
size_t   replay_len = 0;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  char mode_str[12];
  switch (RELAY_MODE) {
    case MODE_HONEST:  strcpy(mode_str, "HONEST");  break;
    case MODE_DROP:    snprintf(mode_str, sizeof(mode_str), "DROP%d%%", DROP_RATE_PCT); break;
    case MODE_BITFLIP: strcpy(mode_str, "BITFLIP"); break;
    case MODE_REPLAY:  strcpy(mode_str, "REPLAY");  break;
  }
  oled.drawString(0,  0, String("RELAY E20 ") + mode_str);
  char buf[40];
  snprintf(buf, sizeof(buf), "rx=%u fwd=%u", frags_rx, frags_forwarded);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "drop=%u flip=%u rep=%u", frags_dropped, frags_flipped, frags_replayed);
  oled.drawString(0, 28, buf);
  oled.display();
}

bool forward_pkt(uint8_t *buf, size_t len) {
  txDone = false;
  radio->setDio1Action(onTxDone_);
  int state = radio->startTransmit(buf, len);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[ERR] TX state=%d\n", state);
    radio->setDio1Action(nullptr);
    return false;
  }
  uint32_t deadline = millis() + 4000;
  while (!txDone && millis() < deadline) delay(1);
  radio->finishTransmit();
  radio->setDio1Action(nullptr);
  return txDone;
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.printf("=== E20 Node Relay (mode=%d) ===\n", RELAY_MODE);
  Wire.begin(18, 17);
  oled.init(); oled.flipScreenVertically(); oled_draw();

  pinMode(LORA_NSS, OUTPUT); digitalWrite(LORA_NSS, HIGH);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_RST, OUTPUT);
  digitalWrite(LORA_RST, LOW); delay(2);
  digitalWrite(LORA_RST, HIGH); delay(20);
  uint32_t t = millis();
  while (digitalRead(LORA_BUSY) && millis()-t < 500) delay(1);

  spi.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  mod   = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, spi);
  radio = new SX1262(mod);
  radio->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
               LORA_SYNC, LORA_POWER, 8, LORA_TCXO);
  radio->setDio1Action(onDio1);
  radio->startReceive();
  Serial.println("[RADIO] relay listening");
}

void loop() {
  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[255];
  size_t  len   = radio->getPacketLength();
  float   rssi  = radio->getRSSI();
  int     state = radio->readData(buf, sizeof(buf));
  radio->setDio1Action(onDio1);
  radio->startReceive();

  if (state != RADIOLIB_ERR_NONE || len < FRAG_HEADER_SIZE + HMAC_TRUNC_SIZE + 1) return;
  if (buf[0] != E19_MAGIC_0 || buf[1] != E19_MAGIC_1) return;

  uint16_t frag_id     = ((uint16_t)buf[2] << 8) | buf[3];
  uint16_t total_frags = ((uint16_t)buf[4] << 8) | buf[5];
  uint8_t  flags       = buf[6];
  bool     is_last     = (flags & 0x02);
  frags_rx++;

  // Store frag_id=0 for replay attack
  if (frag_id == 0) {
    memcpy(replay_buf, buf, len);
    replay_len = len;
  }

  bool do_drop  = false;
  bool do_flip  = false;

  switch (RELAY_MODE) {
    case MODE_HONEST:
      break;

    case MODE_DROP:
      if (random(100) < DROP_RATE_PCT) {
        do_drop = true;
      }
      break;

    case MODE_BITFLIP:
      if (frags_rx % BITFLIP_INTERVAL == 0) {
        do_flip = true;
      }
      break;

    case MODE_REPLAY:
      // Normal forwarding; replay added after image complete
      break;
  }

  if (do_drop) {
    frags_dropped++;
    Serial.printf("[DROP] frag=%u/%u (mode=DROP%d%%)\n",
                  frag_id+1, total_frags, DROP_RATE_PCT);
  } else {
    if (do_flip) {
      size_t flip_pos = FRAG_HEADER_SIZE + BITFLIP_OFFSET;
      if (flip_pos < len - HMAC_TRUNC_SIZE) {
        buf[flip_pos] ^= 0xFF;
        frags_flipped++;
        Serial.printf("[FLIP] frag=%u/%u byte[%u] flipped\n",
                      frag_id+1, total_frags, BITFLIP_OFFSET);
      }
    }

    // Translate sender magic (0xAA) → receiver magic (0x9A) before forwarding
    buf[1] = FWD_MAGIC_1;

    if (forward_pkt(buf, len)) {
      frags_forwarded++;
      Serial.printf("[FWD] frag=%u/%u len=%u rssi=%.1f%s\n",
                    frag_id+1, total_frags, len, rssi,
                    do_flip ? " [FLIPPED]" : "");
    }
    // Re-arm RX interrupt after TX (forward_pkt sets DIO1 to onTxDone_ then nullptr)
    radio->setDio1Action(onDio1);
    radio->startReceive();
    // Restore magic for replay buffer consistency
    buf[1] = E19_MAGIC_1;
  }

  // After last fragment: replay attack
  if (is_last) {
    img_count++;
    Serial.printf("[IMG_DONE] img=%u rx=%u fwd=%u drop=%u flip=%u\n",
                  img_count, frags_rx, frags_forwarded, frags_dropped, frags_flipped);

    if (RELAY_MODE == MODE_REPLAY && replay_len > 0) {
      delay(500);
      replay_buf[1] = FWD_MAGIC_1;  // translate magic for receiver
      Serial.printf("[REPLAY] replaying frag_id=0 (%u bytes)\n", replay_len);
      if (forward_pkt(replay_buf, replay_len)) {
        frags_replayed++;
        Serial.printf("[REPLAY] sent — receiver should detect HMAC mismatch\n");
      }
      // Re-arm RX after replay TX (forward_pkt leaves DIO1=nullptr)
      radio->setDio1Action(onDio1);
      radio->startReceive();
    }
  }

  oled_draw();
}
