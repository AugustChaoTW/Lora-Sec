// E14: RSSI-based Attacker Localization
//
// 5 honest nodes at known bench positions record RSSI of attacker's Hello.
// Python script collects RSSI via UART and runs multilateration.
//
// Grid layout (cm from origin, 2m×2m bench):
//   Node A (0xAA): (  0,   0) — corner
//   Node B (0xBB): (200,   0) — corner
//   Node C (0xCC): (100, 100) — center
//   Node D (0xDD): (  0, 200) — corner
//   Node E (0xEE): (200, 200) — corner
//   Attacker (0xFF): unknown position — broadcast Hello every 2s
//
// RSSI report format (via Serial):
//   RSSI_REPORT,<node_id>,<seq>,<attacker_rssi_dBm>
//
// Python multilateration (collect_rssi.py):
//   path loss model: RSSI = RSSI_ref - 10*n*log10(d/d_ref)
//   solve least squares for (x_att, y_att) from 5 distance estimates
//
// Board: Ebyte EoRa PI (ESP32-S3FH4R2 + SX1262)
// Reference: extends repetto2022-lorawan-demo (UAV+SDR) to static bench + SX1262

#pragma once
#include <RadioLib.h>
#include <SPI.h>

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

#define ID_ATTACKER   0xFF

#define MSG_HELLO_LOC  0x10    // localization-only Hello (no HMAC, short)

// Localization Hello: minimal packet for RSSI measurement
struct LocHello {
  uint8_t  type;    // MSG_HELLO_LOC
  uint8_t  src;
  uint16_t seq;
  uint8_t  tx_power_dbm;   // attacker reports its TX power for path loss calibration
} __attribute__((packed));

// Path loss model parameters (calibrate from known-distance measurements)
// RSSI = RSSI_ref - 10 * n * log10(d / d_ref)
// Defaults for indoor bench environment:
#define PL_RSSI_REF   -40.0f    // RSSI at d_ref (dBm) — calibrate empirically
#define PL_N           2.0f     // path loss exponent (2.0 = free space)
#define PL_D_REF       0.1f     // reference distance (m) — 10 cm
