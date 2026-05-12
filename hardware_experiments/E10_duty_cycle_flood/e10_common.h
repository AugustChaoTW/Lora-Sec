// E10: Duty Cycle Exhaustion Attack (AS923 1% limit)
//
// AS923 imposes 1% duty cycle per sub-band (923.2–924.6 MHz).
// SX1262 does NOT enforce duty cycle in hardware — it is left to firmware.
// LoRaMesher does not implement duty cycle tracking.
//
// Attack: flood Hello packets at maximum rate, consuming the attacker's
// 1% DC budget. This forces honest nodes to process Hellos, update routes,
// and re-broadcast (each re-broadcast consumes the honest node's own DC).
//
// Airtime budget (AS923, SF9 BW125):
//   HelloMsg (24 bytes) → ~185ms airtime
//   1% DC on 1-MHz sub-band → 10ms available per second
//   Actual achievable rate: ~1 Hello per 18.5s per attacker
//
// With multiple sub-bands (AS923 has 8 channels, 200 kHz each):
//   attacker can rotate channels to inject faster
//   honest nodes' response (route update + re-broadcast) adds to their DC
//
// This firmware measures:
//   - Attacker: actual packets sent per minute
//   - Honest node: duty cycle consumption rate (millis in TX / total millis)
//   - Gateway: PDR degradation as honest nodes hit DC limit and stop responding
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

// Measured SF9 BW125 airtime for HelloMsg (24 bytes + preamble + header)
#define HELLO_AIRTIME_MS   185
#define DC_WINDOW_MS      1000    // 1-second duty cycle window
#define DC_LIMIT_PERCENT     1    // AS923 1% limit
// DC budget per window: 1000 * 1% = 10ms
// So honest node can TX at most floor(10/185) = 0 per second per channel
// In practice: 1 packet per 18.5s sustained

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
