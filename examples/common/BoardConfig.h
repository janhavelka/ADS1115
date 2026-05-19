/**
 * @file BoardConfig.h
 * @brief Example board configuration for ESP32-S2 / ESP32-S3 reference hardware.
 *
 * These are convenience defaults for reference designs only.
 * NOT part of the library API. Override for your hardware.
 *
 * @warning The library itself is board-agnostic. All pins are passed via Config.
 *          These defaults are provided for examples only.
 */

#pragma once

#include <stdint.h>

#if defined(ADS1115_EXAMPLE_PLATFORM_IDF)
#include "driver/gpio.h"
#include "examples/common/IdfArduinoCompat.h"
#else
#include <Arduino.h>
#endif

#include "examples/common/I2cTransport.h"

namespace board {

// ====================================================================
// EXAMPLE DEFAULTS - ESP32-S2 / ESP32-S3 REFERENCE HARDWARE
// ====================================================================
// These values are NOT library defaults. They are example-only values.
// Override them for your board by creating your own BoardConfig.h or
// passing explicit values to Config structs in your application.
// ====================================================================

/// @brief I2C SDA pin (data line). Example default for ESP32-S2/S3.
static constexpr int I2C_SDA = 8;

/// @brief I2C SCL pin (clock line). Example default for ESP32-S2/S3.
static constexpr int I2C_SCL = 9;

/// @brief I2C clock frequency in Hz.
static constexpr uint32_t I2C_FREQ_HZ = 400000;

/// @brief I2C timeout in milliseconds for example transactions.
static constexpr uint16_t I2C_TIMEOUT_MS = 50;

/// @brief ADS1115 7-bit I2C address used by the reference example.
static constexpr uint8_t ADS1115_I2C_ADDR = 0x48;

/// @brief LED pin. Example default for ESP32-S3 (RGB LED on GPIO48).
/// Set to -1 to disable.
static constexpr int LED = 48;

/// @brief ALERT/RDY pin from ADS1115 (open-drain).
/// Set to -1 to disable.
static constexpr int ALERT_RDY_PIN = -1;

/// @brief Initialize I2C for examples using the default config.
inline bool initI2c() {
  return transport::initWire(I2C_SDA, I2C_SCL, I2C_FREQ_HZ, I2C_TIMEOUT_MS,
                             ADS1115_I2C_ADDR);
}

/// @brief Initialize ALERT/RDY pin for examples.
inline void initAlertRdyPin() {
  if (ALERT_RDY_PIN >= 0) {
#if defined(ADS1115_EXAMPLE_PLATFORM_IDF)
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << static_cast<uint32_t>(ALERT_RDY_PIN);
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    (void)gpio_config(&cfg);
#else
    pinMode(ALERT_RDY_PIN, INPUT_PULLUP);
#endif
  }
}

/// @brief Read ALERT/RDY pin level (true = HIGH, false = LOW).
inline bool readAlertRdyPin(int pin, void* user) {
  (void)user;
#if defined(ADS1115_EXAMPLE_PLATFORM_IDF)
  return gpio_get_level(static_cast<gpio_num_t>(pin)) != 0;
#else
  return digitalRead(pin) != 0;
#endif
}

/// @brief Initialize Serial for examples.
inline void initSerial(uint32_t baud = 115200) {
  Serial.begin(baud);
  // Allow native USB CDC targets (ESP32-S2/S3) to enumerate before first log.
  const uint32_t startMs = millis();
  while (!Serial && (millis() - startMs) < 3000U) {
    delay(10);
  }
}

}  // namespace board
