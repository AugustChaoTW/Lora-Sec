// E19 — Node RX: FC-HMAC Verifier
//
// Receives fragments, reconstructs image, verifies FC-HMAC chain.
// Reports: chain verification rate, HMAC fail count, PDR.
//
// Hardware: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "mbedtls/md.h"

#define LORA_NSS    7
#define LORA_DIO1  33
#define LORA_RST    8
#define LORA_BUSY  34
#define LORA_SCK    5
#define LORA_MOSI   6
#define LORA_MISO   3
#define LORA_FREQ  922.0  // different from E16/E17/E18 (923.0) to avoid RF collision
#define LORA_BW    125.0
#define LORA_SF      9
#define LORA_CR      7
#define LORA_SYNC  0x34   // standard private-network sync (matches TX)
#define LORA_POWER  10
#define LORA_TCXO  1.6f

#define E19_MAGIC_0     0xE1
#define E19_MAGIC_1     0x9A
#define FRAG_HEADER_SIZE  7   // magic(2)+frag_id(2)+total_frags(2)+flags(1)
#define HMAC_TRUNC_SIZE   8
#define LORA_MAX_PAYLOAD  233
#define MAX_FRAGS         32   // max fragments per image (235B×32 ≈ 7.5KB)

static const uint8_t SESSION_KEY[16] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
};

// Per-image state
uint16_t expected_total = 0;
uint16_t frags_received = 0;
uint16_t frags_verified = 0;
uint16_t frags_failed   = 0;
uint8_t  prev_hmac[8]   = {0};
bool     chain_broken   = false;
uint32_t img_seq        = 0;

// Running totals
uint32_t total_received = 0;
uint32_t total_verified = 0;
uint32_t total_failed   = 0;

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;
volatile bool rxFlag = false;

void IRAM_ATTR onDio1() { rxFlag = true; }

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE RX  E19");
  char buf[40];
  snprintf(buf, sizeof(buf), "rx=%u ok=%u fail=%u",
           total_received, total_verified, total_failed);
  oled.drawString(0, 14, buf);
  float vr = total_received > 0 ?
             (float)total_verified / total_received * 100.0f : 0.0f;
  snprintf(buf, sizeof(buf), "chain_ok=%.0f%%", vr);
  oled.drawString(0, 28, buf);
  oled.display();
}

void fc_hmac_verify(uint16_t frag_id, uint8_t flags,
                    const uint8_t *payload, size_t plen,
                    const uint8_t *prev_h, uint8_t out[8]) {
  uint8_t full[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, SESSION_KEY, 16);
  uint8_t id_buf[2] = {(uint8_t)(frag_id >> 8), (uint8_t)(frag_id & 0xFF)};
  mbedtls_md_hmac_update(&ctx, id_buf, 2);
  mbedtls_md_hmac_update(&ctx, &flags, 1);
  mbedtls_md_hmac_update(&ctx, payload, plen);
  mbedtls_md_hmac_update(&ctx, prev_h, 8);
  mbedtls_md_hmac_finish(&ctx, full);
  mbedtls_md_free(&ctx);
  memcpy(out, full, 8);
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.println("=== E19 Node RX (FC-HMAC Verifier) ===");
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
  Serial.println("[RADIO] listening for FC-HMAC fragments");
}

void loop() {
  if (!rxFlag) return;
  rxFlag = false;

  uint8_t buf[255];
  size_t  len   = radio->getPacketLength();
  float   rssi  = radio->getRSSI();
  int     state = radio->readData(buf, sizeof(buf));
  radio->startReceive();  // restart RX immediately; DIO1 action stays from setup

  if (state != RADIOLIB_ERR_NONE || len < FRAG_HEADER_SIZE + HMAC_TRUNC_SIZE + 1) {
    Serial.printf("[DROP] state=%d len=%u\n", state, len);
    return;
  }

  // Verify E19 magic header to filter non-E19 traffic
  if (buf[0] != E19_MAGIC_0 || buf[1] != E19_MAGIC_1) {
    Serial.printf("[DROP] bad magic %02X%02X\n", buf[0], buf[1]);
    return;
  }

  uint16_t frag_id     = ((uint16_t)buf[2] << 8) | buf[3];
  uint16_t total_frags = ((uint16_t)buf[4] << 8) | buf[5];
  uint8_t  flags       = buf[6];
  size_t   plen        = len - FRAG_HEADER_SIZE - HMAC_TRUNC_SIZE;
  uint8_t  *payload    = buf + FRAG_HEADER_SIZE;
  uint8_t  *rcv_hmac   = buf + FRAG_HEADER_SIZE + plen;

  total_received++;

  // First fragment: init chain
  if (frag_id == 0) {
    expected_total  = total_frags;
    frags_received  = 0;
    frags_verified  = 0;
    frags_failed    = 0;
    chain_broken    = false;
    memset(prev_hmac, 0, 8);
    // Recompute session init HMAC (must match sender: only total_frags, bytes [0..3]=0)
    uint8_t sess_data[6];
    memset(sess_data, 0, sizeof(sess_data));
    sess_data[4] = (total_frags >> 8) & 0xFF; sess_data[5] = total_frags & 0xFF;
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    uint8_t full[32];
    mbedtls_md_hmac_starts(&ctx, SESSION_KEY, 16);
    mbedtls_md_hmac_update(&ctx, sess_data, 6);
    mbedtls_md_hmac_finish(&ctx, full);
    mbedtls_md_free(&ctx);
    memcpy(prev_hmac, full, 8);
    Serial.printf("[NEW_IMG] total_frags=%u\n", total_frags);
  }

  frags_received++;

  // Verify FC-HMAC
  uint8_t computed[8];
  fc_hmac_verify(frag_id, flags, payload, plen, prev_hmac, computed);
  bool ok = (memcmp(computed, rcv_hmac, 8) == 0);

  if (ok) {
    frags_verified++;
    total_verified++;
    memcpy(prev_hmac, rcv_hmac, 8);
    Serial.printf("[OK] frag=%u/%u plen=%u rssi=%.1f hmac=OK\n",
                  frag_id+1, total_frags, plen, rssi);
  } else {
    frags_failed++;
    total_failed++;
    chain_broken = true;
    Serial.printf("[FAIL] frag=%u/%u HMAC MISMATCH computed=%02X%02X rcv=%02X%02X\n",
                  frag_id+1, total_frags,
                  computed[0], computed[1], rcv_hmac[0], rcv_hmac[1]);
  }

  // Image complete
  if (flags & 0x02) {
    float vr = frags_received > 0 ?
               (float)frags_verified / frags_received * 100.0f : 0.0f;
    Serial.printf("[IMG_DONE] seq=%u recv=%u/%u verified=%u failed=%u "
                  "chain_ok=%.0f%% chain_broken=%s\n",
                  img_seq, frags_received, expected_total,
                  frags_verified, frags_failed, vr,
                  chain_broken ? "YES" : "NO");
    Serial.printf("[OVERHEAD] hmac_bytes=%u / img_payload_bytes~=%u = %.1f%%\n",
                  (uint32_t)HMAC_TRUNC_SIZE * frags_received,
                  (uint32_t)plen * frags_received,
                  (float)HMAC_TRUNC_SIZE / (LORA_MAX_PAYLOAD + HMAC_TRUNC_SIZE) * 100.0f);
    img_seq++;
  }

  oled_draw();
}
