// E9: Sybil Address Exhaustion Attack
//
// LoRaMesher routing table is bounded (MAX_ROUTES=8 in our implementation,
// ~10 in upstream LoRaMesher based on ESP32 SRAM constraints).
// Attacker broadcasts Hello messages from rapidly-cycling fake source
// addresses, filling the routing table with phantom routes.
// When table is full, legitimate gateway route gets evicted → PDR=0%.
//
// Attack: broadcast Hello(src=0xD0..0xFF cycling, metric=1, HMAC=valid)
//   Each unique src creates one routing table entry on honest nodes.
//   After MAX_ROUTES unique fakes, legitimate NH=0xBB is evicted.
//
// Note: With HMAC patch (SECURITY_MODE=1), attacker MUST hold valid PSK
// to sign each Hello (insider threat again). Without HMAC (mode=0), no PSK needed.
//
// Experiment runs BOTH modes to show:
//   mode=0: Sybil works trivially (unsigned Hellos accepted)
//   mode=1: Sybil still works if attacker is insider (holds PSK)
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2 + SX1262)
// Freq:  923 MHz AS923

#pragma once
#include <RadioLib.h>
#include <SPI.h>
#include "mbedtls/md.h"

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

#define HMAC_KEY_LEN  16
#define HMAC_TAG_LEN  16

static const uint8_t NETWORK_PSK[HMAC_KEY_LEN] = {
  0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

#define ID_SENDER   0xAA
#define ID_GATEWAY  0xBB
#define ID_ATTACKER 0xCC
#define ID_R1       0x11

#define MSG_HELLO   0x01
#define MSG_DATA    0x02
#define MSG_ACK     0x03

// Sybil address range: 0xD0–0xFF (48 fake addresses, far more than MAX_ROUTES=8)
#define SYBIL_ADDR_START  0xD0
#define SYBIL_ADDR_END    0xFF
#define SYBIL_ADDR_COUNT  (SYBIL_ADDR_END - SYBIL_ADDR_START + 1)

// MAX_ROUTES: routing table capacity on honest nodes
// LoRaMesher upstream uses ~10; our experiment common.h uses 8
#define MAX_ROUTES_ESTIMATE  8

struct HelloMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst_gw;
  uint8_t  metric;
  uint16_t seq;
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct DataMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst;
  uint8_t  final_dst;
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

#define HELLO_PERIOD_MS   10000
#define DATA_PERIOD_MS     5000

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
