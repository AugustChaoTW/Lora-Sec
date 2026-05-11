// E6: 6-Node Linear Topology Demo
// Extends E5c with relay nodes to create a true multi-hop chain.
//
// Topology: A(0xAA) → R1(0x11) → R2(0x22) → R3(0x33) → B(0xBB)
//           Attacker C(0xCC) attacks at R2/R3 position
//
// SECURITY_MODE:
//   0 = no auth  — injection succeeds
//   1 = HMAC     — injection blocked
//
// Software-enforced neighbor whitelist simulates linear topology
// on bench hardware where all nodes can hear each other.
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2 + E22-900MM22S / SX1262)
// Freq:  923 MHz AS923 (Taiwan)

#pragma once
#include <RadioLib.h>
#include <SPI.h>
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

// ----- Node IDs -----
#define ID_SENDER   0xAA
#define ID_R1       0x11
#define ID_R2       0x22
#define ID_R3       0x33
#define ID_GATEWAY  0xBB
#define ID_ATTACKER 0xCC

// ----- Message types -----
#define MSG_HELLO   0x01
#define MSG_DATA    0x02
#define MSG_ACK     0x03

// ----- Message structures -----
struct HelloMsg {
  uint8_t  type;
  uint8_t  src;        // original sender of this Hello
  uint8_t  dst_gw;     // final gateway
  uint8_t  metric;     // hop count to gateway
  uint16_t seq;
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct DataMsg {
  uint8_t  type;
  uint8_t  src;        // originator
  uint8_t  dst;        // next hop
  uint8_t  final_dst;  // final gateway (0xBB)
  uint16_t pkt_id;
  uint8_t  payload[8];
  uint8_t  hmac[HMAC_TAG_LEN];
} __attribute__((packed));

struct AckMsg {
  uint8_t  type;
  uint8_t  src;
  uint8_t  dst;        // original sender (0xAA)
  uint16_t pkt_id;
} __attribute__((packed));

// ----- Timing -----
#define HELLO_PERIOD_MS   10000
#define DATA_PERIOD_MS     5000
#define ROUTE_EXPIRE_MS  300000

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
