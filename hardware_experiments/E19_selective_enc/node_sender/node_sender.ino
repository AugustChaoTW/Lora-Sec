// E19 — Node TX: Selective Encryption + FC-HMAC Sender
//
// Transmits a synthetic JPEG (embedded byte array) over LoRa using:
//   1. Selective encryption: HMAC-SHA256-based stream cipher on DC bytes only
//      (Hardware AES-CTR omitted: ESP32-S3 Arduino HW AES semaphore assert;
//       HMAC-SHA256 as PRF generates equivalent keystream, software-only)
//   2. Fragment-Chained HMAC (FC-HMAC): 8-byte truncated HMAC chain
//
// Metrics collected:
//   - Airtime per fragment (us, from TX start to TX done)
//   - CPU cycles for HMAC + enc per byte
//   - Payload overhead: HMAC bytes / total bytes
//
// Hardware: Ebyte EoRa PI (ESP32-S3FH4R2)  FQBN: esp32:esp32:esp32s3:CDCOnBoot=cdc

#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include "SSD1306Wire.h"
#include "mbedtls/md.h"
#include "test_image.h"   // embedded JPEG (copy to sketch dir)

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
#define LORA_SYNC  0x34   // standard private-network sync (0xE9 caused SX1262 CRC errors)
#define LORA_POWER   2    // low power: boards nearby, 10dBm RSSI=-17dBm near SX1262 max
#define LORA_TCXO  1.6f

// Packet layout: | magic(2B:E1,9A) | frag_id(2B) | total_frags(2B) | flags(1B) | payload(N) | hmac8(8B) |
#define E19_MAGIC_0   0xE1
#define E19_MAGIC_1   0x9A
#define FRAG_HEADER_SIZE  7   // magic(2)+frag_id(2)+total_frags(2)+flags(1)
#define HMAC_TRUNC_SIZE   8   // 64-bit truncated HMAC
#define LORA_MAX_PAYLOAD  233 // 233B payload + 7B header + 8B HMAC = 248B < 255B
#define FRAG_PAYLOAD_SIZE LORA_MAX_PAYLOAD

// Selective encryption mode
#define ENC_NONE    0
#define ENC_FULL    1
#define ENC_DC_ONLY 2

#define ENC_MODE ENC_DC_ONLY

// Pre-shared session key (16 bytes, 128-bit AES)
static const uint8_t SESSION_KEY[16] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE,
  0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF
};

// TX interrupt flag (interrupt-driven TX avoids FreeRTOS queue assert from blocking transmit())
volatile bool txDoneFlag = false;
void IRAM_ATTR onTxDone() { txDoneFlag = true; }

// Stats
uint32_t frags_sent    = 0;
uint32_t total_airtime = 0;   // us
uint32_t hmac_cpu_us   = 0;
uint32_t aes_cpu_us    = 0;
uint32_t image_seq     = 0;
uint8_t  prev_hmac[8]  = {0};

SSD1306Wire oled(0x3c, 18, 17);
SPIClass    spi(FSPI);
Module*     mod   = nullptr;
SX1262*     radio = nullptr;

void oled_draw() {
  oled.clear();
  oled.setFont(ArialMT_Plain_10);
  oled.setTextAlignment(TEXT_ALIGN_LEFT);
  oled.drawString(0,  0, "NODE TX  E19");
  char buf[40];
  snprintf(buf, sizeof(buf), "img=%u frags=%u", image_seq, frags_sent);
  oled.drawString(0, 14, buf);
  snprintf(buf, sizeof(buf), "air=%ums", total_airtime/1000);
  oled.drawString(0, 28, buf);
  oled.drawString(0, 42,
    ENC_MODE == ENC_DC_ONLY ? "DC-ONLY" :
    ENC_MODE == ENC_FULL    ? "FULL" : "NONE");
  oled.display();
}

// Compute FC-HMAC: HMAC-SHA256(key, frag_id||flags||payload||prev_hmac)[:8]
void fc_hmac(uint16_t frag_id, uint8_t flags, const uint8_t *payload, size_t plen,
             const uint8_t *prev_h, uint8_t out[8]) {
  uint32_t t0 = micros();
  uint8_t full_hmac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, SESSION_KEY, 16);
  uint8_t id_buf[2] = {(uint8_t)(frag_id >> 8), (uint8_t)(frag_id & 0xFF)};
  mbedtls_md_hmac_update(&ctx, id_buf, 2);
  mbedtls_md_hmac_update(&ctx, &flags, 1);
  mbedtls_md_hmac_update(&ctx, payload, plen);
  mbedtls_md_hmac_update(&ctx, prev_h, 8);
  mbedtls_md_hmac_finish(&ctx, full_hmac);
  mbedtls_md_free(&ctx);
  memcpy(out, full_hmac, 8);
  hmac_cpu_us += micros() - t0;
}

// Selective encryption: HMAC-SHA256 stream cipher (software-only, no HW AES)
// DC_ONLY: XOR 16 bytes at every 64-byte block boundary (DC coefficient approximation)
// FULL:    XOR entire buffer in 32-byte keystream blocks
// Hardware AES omitted: ESP32-S3 Arduino HW AES semaphore crashes Arduino framework
static void hmac_prf_keystream(uint32_t nonce, uint32_t pos, uint8_t out32[32]) {
  uint8_t prf_in[8] = {
    (uint8_t)(nonce >> 24), (uint8_t)(nonce >> 16),
    (uint8_t)(nonce >>  8), (uint8_t)(nonce),
    (uint8_t)(pos   >> 24), (uint8_t)(pos   >> 16),
    (uint8_t)(pos   >>  8), (uint8_t)(pos)
  };
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, SESSION_KEY, 16);
  mbedtls_md_hmac_update(&ctx, prf_in, 8);
  mbedtls_md_hmac_finish(&ctx, out32);
  mbedtls_md_free(&ctx);
}

void selective_enc(uint8_t *data, size_t len, uint8_t mode, uint32_t nonce) {
  if (mode == ENC_NONE) return;
  uint32_t t0 = micros();
  uint8_t ks[32];

  if (mode == ENC_DC_ONLY) {
    for (size_t i = 0; i < len; i += 64) {
      hmac_prf_keystream(nonce, (uint32_t)i, ks);
      size_t blk = ((i + 16) < len) ? 16 : (len - i);
      for (size_t j = 0; j < blk; j++) data[i+j] ^= ks[j];
    }
  } else if (mode == ENC_FULL) {
    for (size_t i = 0; i < len; i += 32) {
      hmac_prf_keystream(nonce, (uint32_t)i, ks);
      size_t blk = ((i + 32) < len) ? 32 : (len - i);
      for (size_t j = 0; j < blk; j++) data[i+j] ^= ks[j];
    }
  }

  aes_cpu_us += micros() - t0;
}

void send_image() {
  // Work on a local copy of the image
  size_t img_size = TEST_IMG_SIZE;
  uint8_t *img = (uint8_t*)malloc(img_size);
  if (!img) { Serial.println("[ERR] malloc failed"); return; }
  memcpy_P(img, test_image, img_size);

  Serial.printf("\n[IMG] seq=%u size=%u enc_mode=%d\n", image_seq, img_size, ENC_MODE);

  // Apply selective encryption to full image buffer
  selective_enc(img, img_size, ENC_MODE, image_seq);

  // Fragment + FC-HMAC + transmit
  uint16_t total_frags = (img_size + FRAG_PAYLOAD_SIZE - 1) / FRAG_PAYLOAD_SIZE;
  memset(prev_hmac, 0, 8);

  // Initialize chain with session header HMAC
  // NOTE: image_seq omitted from sess_data — F3 (replay resistance) not enforced here.
  // F3 requires image_seq in init; fixed in E20 firmware. See GitHub #62.
  {
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
  }

  uint8_t pkt[255];
  uint32_t img_airtime = 0;

  for (uint16_t frag = 0; frag < total_frags; frag++) {
    size_t offset = frag * FRAG_PAYLOAD_SIZE;
    size_t plen   = (offset + FRAG_PAYLOAD_SIZE <= img_size)
                    ? FRAG_PAYLOAD_SIZE : (img_size - offset);
    uint8_t flags = (frag == 0) ? 0x01 : (frag == total_frags-1) ? 0x02 : 0x00;

    // Build packet: magic | frag_id | total_frags | flags | payload | hmac8
    pkt[0] = E19_MAGIC_0; pkt[1] = E19_MAGIC_1;
    pkt[2] = (frag >> 8) & 0xFF; pkt[3] = frag & 0xFF;
    pkt[4] = (total_frags >> 8) & 0xFF; pkt[5] = total_frags & 0xFF;
    pkt[6] = flags;
    memcpy(pkt + FRAG_HEADER_SIZE, img + offset, plen);

    // FC-HMAC
    uint8_t hmac8[8];
    fc_hmac(frag, flags, img + offset, plen, prev_hmac, hmac8);
    memcpy(pkt + FRAG_HEADER_SIZE + plen, hmac8, 8);
    memcpy(prev_hmac, hmac8, 8);

    size_t pkt_size = FRAG_HEADER_SIZE + plen + HMAC_TRUNC_SIZE;

    // Interrupt-driven TX: avoids blocking transmit() FreeRTOS queue assert
    txDoneFlag = false;
    radio->setDio1Action(onTxDone);
    uint32_t t_tx = micros();
    int tx_state = radio->startTransmit(pkt, pkt_size);
    if (tx_state != RADIOLIB_ERR_NONE) {
      Serial.printf("[ERR] startTransmit state=%d\n", tx_state);
      radio->setDio1Action(nullptr);
      delay(100);
      continue;
    }
    // Wait for TX done interrupt (4s timeout for SF9 worst-case ~1.9s airtime)
    uint32_t t_deadline = millis() + 4000;
    while (!txDoneFlag && millis() < t_deadline) { delay(1); }
    uint32_t dt = micros() - t_tx;
    radio->finishTransmit();
    radio->setDio1Action(nullptr);

    img_airtime   += dt;
    total_airtime += dt;
    frags_sent++;

    Serial.printf("[TX] frag=%u/%u pkt=%uB air=%uus hmac=%02X%02X%02X%02X%02X%02X%02X%02X\n",
                  frag+1, total_frags, pkt_size, dt,
                  hmac8[0], hmac8[1], hmac8[2], hmac8[3],
                  hmac8[4], hmac8[5], hmac8[6], hmac8[7]);

    delay(100); // inter-frame gap (100ms: allow RX to process + SX1262 settle)
  }

  free(img);
  float overhead_pct = 100.0f * HMAC_TRUNC_SIZE * total_frags / img_size;
  Serial.printf("[DONE] img=%u frags=%u total_air=%ums hmac_cpu=%uus enc_cpu=%uus overhead=%.1f%%\n",
                image_seq, total_frags, img_airtime/1000,
                hmac_cpu_us, aes_cpu_us, overhead_pct);
  Serial.printf("[ENC] mode=%s enc_bytes~=%u/%u (%.0f%%)\n",
                ENC_MODE == ENC_DC_ONLY ? "DC_ONLY" : ENC_MODE == ENC_FULL ? "FULL" : "NONE",
                ENC_MODE == ENC_DC_ONLY ? img_size/64*16 : (ENC_MODE==ENC_FULL ? img_size : 0),
                img_size,
                ENC_MODE == ENC_DC_ONLY ? (float)(img_size/64*16)/img_size*100 :
                ENC_MODE == ENC_FULL ? 100.0f : 0.0f);
  image_seq++;
  hmac_cpu_us = 0;
  aes_cpu_us  = 0;
}

void setup() {
  Serial.begin(115200); delay(500);
  Serial.printf("=== E19 Node TX (Selective Enc + FC-HMAC) ===\n");
  Serial.printf("[CONFIG] enc_mode=%d frag_payload=%dB hmac=%dB img_size=%dB\n",
                ENC_MODE, FRAG_PAYLOAD_SIZE, HMAC_TRUNC_SIZE, TEST_IMG_SIZE);
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
  Serial.println("[RADIO] ready");
}

void loop() {
  static uint32_t last_tx = 0;
  if (millis() - last_tx < 10000) return;
  last_tx = millis();

  send_image();
  oled_draw();
}
