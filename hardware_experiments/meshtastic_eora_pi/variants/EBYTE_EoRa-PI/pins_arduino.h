// Need this file for ESP32-S3
// Identical pattern to CDEBYTE_EoRa-S3/pins_arduino.h —
// no changes needed because all pin symbols come from variant.h

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include <variant.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

// Serial
static const uint8_t TX = UART_TX;
static const uint8_t RX = UART_RX;

// Default SPI mapped to Radio
static const uint8_t SS   = LORA_CS;
static const uint8_t SCK  = LORA_SCK;
static const uint8_t MOSI = LORA_MOSI;
static const uint8_t MISO = LORA_MISO;

// Default Wire mapped to OLED
static const uint8_t SCL = I2C_SCL;
static const uint8_t SDA = I2C_SDA;

#endif /* Pins_Arduino_h */
