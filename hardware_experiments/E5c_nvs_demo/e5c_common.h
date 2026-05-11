// E5c: NVS-Persisted MetricVersion Counter Demo
// Extends E5b with Patch 2 (MetricVersion) + NVS flash persistence.
// Board: Ebyte EoRa PI (ESP32-S3FH4R2 + E22-900MM22S / SX1262)
// Freq:  923 MHz (AS923, Taiwan)
//
// SECURITY_MODE:
//   0 = baseline  (no auth — attack succeeds even without reboot)
//   1 = HMAC only (E5b patch — blocks injection, NOT reboot replay)
//   2 = HMAC + MetricVersion + NVS (full patch — reboot window eliminated)

#pragma once
#include <RadioLib.h>
#include <SPI.h>
#include <Preferences.h>
#include "mbedtls/md.h"

// ----- Radio pins (EoRa PI) -----
#define LORA_NSS    7
#define LORA_DIO1  33
#define LORA_RST    8
#define LORA_BUSY  34
#define LORA_SCK    5
#define LORA_MOSI   6
#define LORA_MISO   3

inline void eora_radio_init() {
  pinMode(LORA_NSS,  OUTPUT); digitalWrite(LORA_NSS,  HIGH);
  pinMode(LORA_BUSY, INPUT);
  pinMode(LORA_RST,  OUTPUT);
  digitalWrite(LORA_RST, LOW);  delay(2);
  digitalWrite(LORA_RST, HIGH); delay(20);
  uint32_t t = millis();
  while (digitalRead(LORA_BUSY) && millis() - t < 500) delay(1);
}

#define LORA_FREQ   923.0
#define LORA_BW     125.0
#define LORA_SF     9
#define LORA_CR     7
#define LORA_SYNC   0x34
#define LORA_POWER  10
#define LORA_TCXO   1.6f

// ----- HMAC config -----
#define HMAC_KEY_LEN  16
#define HMAC_TAG_LEN  16

static const uint8_t NETWORK_PSK[HMAC_KEY_LEN] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

// ----- Message types -----
#define MSG_HELLO   0x01
#define MSG_DATA    0x02
#define MSG_ACK     0x03

// ----- Message structures -----
// NOTE: metric_version added before hmac; sizeof(HelloMsg) = 22 bytes
struct HelloMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst_gw;
  uint8_t  metric;
  uint16_t seq;
  uint16_t metric_version;   // Patch 2: monotonic per-sender counter
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct DataMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst;
  uint16_t pkt_id;
  uint8_t  payload[8];
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct AckMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst;
  uint16_t pkt_id;
} __attribute__((packed));

// ----- Routing table -----
struct RouteEntry {
  uint8_t  dst;
  uint8_t  next_hop;
  uint8_t  metric;
  uint32_t last_update_ms;
  uint16_t metric_version;   // highest MV seen from this sender
};
#define MAX_ROUTES       8
#define ROUTE_EXPIRE_MS  300000

// ----- NVS persistence (Preferences wrapper) -----
// Stores per-destination MetricVersion under namespace "e5c".
// Key format: "mv_XX" where XX = dst node ID in hex (e.g. "mv_bb").
static Preferences _prefs;

static char _mv_key[8];
static const char* mv_key(uint8_t dst) {
  snprintf(_mv_key, sizeof(_mv_key), "mv_%02x", dst);
  return _mv_key;
}

static uint16_t nvs_load_mv(uint8_t dst) {
  _prefs.begin("e5c", /*readOnly=*/true);
  uint16_t v = _prefs.getUShort(mv_key(dst), 0);
  _prefs.end();
  return v;
}

static void nvs_save_mv(uint8_t dst, uint16_t mv) {
  _prefs.begin("e5c", /*readOnly=*/false);
  _prefs.putUShort(mv_key(dst), mv);
  _prefs.end();
}

// Call once at boot to print all persisted counters for debugging.
static void nvs_dump(const uint8_t* dst_list, int n) {
  Serial.println("[NVS] Persisted MetricVersion counters:");
  for (int i = 0; i < n; i++) {
    Serial.printf("  mv[0x%02X] = %u\n", dst_list[i], nvs_load_mv(dst_list[i]));
  }
}

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

// Sign covers all fields except hmac (which is zeroed first).
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
