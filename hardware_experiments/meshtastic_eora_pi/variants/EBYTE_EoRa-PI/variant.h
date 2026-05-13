// EBYTE EoRa PI - Meshtastic Board Variant Definition
// MCU: ESP32-S3FH4R2 (4MB flash, 2MB PSRAM)
// Radio: SX1262 (via CDEBYTE E22-900MM22S module on-board)
// Based on: variants/esp32s3/CDEBYTE_EoRa-S3 (upstream develop branch)
// Key difference from EoRa-S3: EoRa PI uses TCXO (DIO3_TCXO_VOLTAGE=1.6f),
//   whereas EoRa-S3 uses XTAL (no TCXO). RF switch topology is the same (DIO2).

#pragma once

// ──────────────────────────────────────────────────────────────────
// Radio – SX1262
// ──────────────────────────────────────────────────────────────────
#define USE_SX1262

#define SX126X_CS    7   // NSS
#define LORA_SCK     5
#define LORA_MOSI    6
#define LORA_MISO    3
#define SX126X_RESET 8
#define SX126X_BUSY  34
#define SX126X_DIO1  33

// DIO2 drives the RF switch (TX/RX path selection), wired internally on EoRa PI.
// Inverted externally, so both TX enable and RX enable are handled automatically.
#define SX126X_DIO2_AS_RF_SWITCH

// EoRa PI uses a TCXO at 1.6 V (unlike EoRa-S3 which has an XTAL).
// RadioLib will assert DIO3 to power the TCXO before each TX/RX.
#define SX126X_DIO3_TCXO_VOLTAGE 1.6f

// Maximum output power from the SX1262 die (E22-900MM22S, 22 dBm rated).
#define SX126X_MAX_POWER 22

// Legacy pin aliases required by sleep.cpp and a few other src files
#define LORA_CS   SX126X_CS
#define LORA_DIO1 SX126X_DIO1

// ──────────────────────────────────────────────────────────────────
// Display – SSD1306 OLED (I2C)
// ──────────────────────────────────────────────────────────────────
#define HAS_SCREEN 1
#define USE_SSD1306
#define I2C_SCL 17
#define I2C_SDA 18

// ──────────────────────────────────────────────────────────────────
// User interface
// ──────────────────────────────────────────────────────────────────
// BOOT button doubles as the Meshtastic user button; has an on-board pull-up.
#define BUTTON_PIN 0

// Status LED on GPIO37 (active-high on EoRa PI carrier board)
#define LED_POWER    37
#define LED_STATE_ON  1

// ──────────────────────────────────────────────────────────────────
// UART (JST SH connector, same as EoRa-S3)
// ──────────────────────────────────────────────────────────────────
#define UART_TX 43
#define UART_RX 44

// ──────────────────────────────────────────────────────────────────
// No GPS, no battery ADC, no SD card on EoRa PI base config
// ──────────────────────────────────────────────────────────────────
#define HAS_GPS 0
// Do NOT define BATTERY_PIN – no voltage divider on EoRa PI
// Do NOT define HAS_SDCARD

// ──────────────────────────────────────────────────────────────────
// USB CDC on Boot (required for Serial output over USB)
// Set in board JSON as ARDUINO_USB_CDC_ON_BOOT=1 / ARDUINO_USB_MODE=0
// ──────────────────────────────────────────────────────────────────
// (No define needed here; controlled via board JSON extra_flags)
