// E5b: Common definitions for all three nodes
// Board: Ebyte EoRa PI (ESP32-S3FH4R2 + E22-900MM22S / SX1262)
// Freq: 923 MHz (AS923, Taiwan)
// Pin source: utilities.h from Tech500/EoRa-PI-Foundation

#pragma once
#include <RadioLib.h>
#include <SPI.h>
#include "mbedtls/md.h"

// ----- Radio config (EoRa PI correct pins) -----
#define LORA_NSS    7
#define LORA_DIO1  33
#define LORA_RST    8
#define LORA_BUSY  34
#define LORA_SCK    5
#define LORA_MOSI   6
#define LORA_MISO   3

// Hard-reset SX126x before SPI init (call in setup() before spi.begin)
inline void eora_radio_init() {
  // NSS must be idle-HIGH before any SPI activity
  pinMode(LORA_NSS,  OUTPUT);  digitalWrite(LORA_NSS,  HIGH);
  // BUSY is driven by the chip — must be INPUT
  pinMode(LORA_BUSY, INPUT);
  // Reset pulse: LOW→HIGH, generous delays so chip startup completes
  pinMode(LORA_RST,  OUTPUT);
  digitalWrite(LORA_RST, LOW);  delay(2);
  digitalWrite(LORA_RST, HIGH); delay(20);
  // Wait for BUSY to go LOW (chip ready, ~3.5 ms typ)
  uint32_t t = millis();
  while (digitalRead(LORA_BUSY) && millis() - t < 500) delay(1);
}

#define LORA_FREQ   923.0   // MHz — AS923, Taiwan
#define LORA_BW     125.0   // kHz
#define LORA_SF     9
#define LORA_CR     7       // 4/7
#define LORA_SYNC   0x34    // LoRa public sync word
#define LORA_POWER  10      // dBm (legal limit AS923)
// E22-900MM22S uses TCXO controlled via DIO3; 1.6V confirmed working
#define LORA_TCXO   1.6f    // DIO3 TCXO supply voltage (V)

// ----- HMAC config -----
#define HMAC_KEY_LEN  16
#define HMAC_TAG_LEN  16

// Pre-shared key (all legitimate nodes share this; attacker does NOT)
static const uint8_t NETWORK_PSK[HMAC_KEY_LEN] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

// ----- Message types -----
#define MSG_HELLO   0x01   // DV routing hello (metric advertisement)
#define MSG_DATA    0x02   // Application data packet
#define MSG_ACK     0x03   // Data acknowledgement

// SECURITY_MODE: 0 = baseline (no auth), 1 = patched (HMAC)
// Set per-sketch in the .ino file

// ----- Message structures -----
struct HelloMsg {
  uint8_t  type;         // MSG_HELLO
  uint8_t  src;          // sender node ID
  uint8_t  dst_gw;       // advertised gateway node ID
  uint8_t  metric;       // hop metric to gateway (lower = better)
  uint16_t seq;          // sequence number
  uint8_t  hmac[HMAC_TAG_LEN]; // zeros if SECURITY_MODE=0
} __attribute__((packed));

struct DataMsg {
  uint8_t  type;         // MSG_DATA
  uint8_t  src;
  uint8_t  dst;
  uint16_t pkt_id;
  uint8_t  payload[8];
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct AckMsg {
  uint8_t  type;         // MSG_ACK
  uint8_t  src;
  uint8_t  dst;
  uint16_t pkt_id;
} __attribute__((packed));

// ----- Routing table (simple: one entry per node) -----
struct RouteEntry {
  uint8_t  dst;
  uint8_t  next_hop;
  uint8_t  metric;
  uint32_t last_update_ms;
};
#define MAX_ROUTES 8
#define ROUTE_EXPIRE_MS 300000

// ----- HMAC helpers -----
static void compute_hmac(const uint8_t *data, size_t dlen,
                         const uint8_t *key, uint8_t *tag_out) {
  mbedtls_md_context_t ctx;
  uint8_t full[32];
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, key, HMAC_KEY_LEN);
  mbedtls_md_hmac_update(&ctx, data, dlen);
  mbedtls_md_hmac_finish(&ctx, full);
  mbedtls_md_free(&ctx);
  memcpy(tag_out, full, HMAC_TAG_LEN);
}

static bool verify_hmac(const uint8_t *data, size_t dlen,
                        const uint8_t *key, const uint8_t *tag) {
  uint8_t expected[HMAC_TAG_LEN];
  compute_hmac(data, dlen, key, expected);
  return memcmp(expected, tag, HMAC_TAG_LEN) == 0;
}

// Sign a HelloMsg in-place (zero out hmac field, then compute over whole struct)
static void sign_hello(HelloMsg *m) {
  memset(m->hmac, 0, HMAC_TAG_LEN);
  compute_hmac((uint8_t*)m, sizeof(HelloMsg) - HMAC_TAG_LEN,
               NETWORK_PSK, m->hmac);
}

static bool check_hello(const HelloMsg *m) {
  uint8_t tmp[sizeof(HelloMsg)];
  memcpy(tmp, m, sizeof(HelloMsg));
  memset(tmp + offsetof(HelloMsg, hmac), 0, HMAC_TAG_LEN);
  return verify_hmac(tmp, sizeof(HelloMsg) - HMAC_TAG_LEN,
                     NETWORK_PSK, m->hmac);
}

static void sign_data(DataMsg *m) {
  memset(m->hmac, 0, HMAC_TAG_LEN);
  compute_hmac((uint8_t*)m, sizeof(DataMsg) - HMAC_TAG_LEN,
               NETWORK_PSK, m->hmac);
}

static bool check_data(const DataMsg *m) {
  uint8_t tmp[sizeof(DataMsg)];
  memcpy(tmp, m, sizeof(DataMsg));
  memset(tmp + offsetof(DataMsg, hmac), 0, HMAC_TAG_LEN);
  return verify_hmac(tmp, sizeof(DataMsg) - HMAC_TAG_LEN,
                     NETWORK_PSK, m->hmac);
}
