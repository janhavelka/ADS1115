/**
 * @file I2cTransport.h
 * @brief Wire-based I2C transport adapter for ADS1115 examples.
 *
 * This file provides Wire-compatible I2C callbacks that can be
 * used with the ADS1115 driver. The library does not depend on Wire
 * directly; this adapter bridges them.
 *
 * NOT part of the library API. Example-only.
 * This is diagnostic Arduino glue, not a production shared-bus manager. A
 * production adapter should implement external bus locking and timeout policy.
 */

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "ADS1115/Status.h"

namespace transport {

/**
 * @brief Apply the driver-supplied per-transfer timeout to this Wire call.
 *
 * The driver partitions each callback's timeout against the remaining operation
 * deadline, so the adapter must apply the supplied value per call instead of
 * relying on a single value latched at initWire() time.
 */
inline void applyWireTimeout(TwoWire* wire, uint32_t timeoutMs) {
#if defined(ARDUINO_ARCH_ESP32)
  const uint32_t clamped = timeoutMs > 0xFFFFU ? 0xFFFFU : timeoutMs;
  wire->setTimeOut(static_cast<uint16_t>(clamped));
#else
  (void)wire;
  (void)timeoutMs;
#endif
}

inline ADS1115::Status mapWireStatus(uint8_t result) {
  switch (result) {
    case 0:
      return ADS1115::Status::Ok();
    case 1:
      return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM,
                                    "I2C data too long", result);
    case 2:
      return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_ADDR,
                                    "I2C address NACK", result);
    case 3:
      return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_DATA,
                                    "I2C data NACK", result);
    case 4:
      return ADS1115::Status::Error(ADS1115::Err::I2C_BUS, "I2C bus error", result);
    case 5:
      return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT,
                                    "I2C timeout", result);
    default:
      return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "I2C unknown error", result);
  }
}

/**
 * @brief Wire-based I2C write implementation.
 *
 * Pass to Config::i2cWrite, and pass &Wire (or custom TwoWire*) to i2cUser.
 *
 * @param addr I2C 7-bit address
 * @param data Data buffer to send
 * @param len Number of bytes
 * @param timeoutMs Timeout requested by the driver (bus-manager owned in shared buses)
 * @param user Pointer to TwoWire instance
 * @return Status OK on success, I2C error on failure
 */
inline ADS1115::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                                 uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "Wire instance is null");
  }
  if (!data || len == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Invalid I2C write params");
  }

  // Check for oversized writes (ESP32 Wire buffer is 128 bytes)
  if (len > 128) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Write exceeds I2C buffer",
                                  static_cast<int32_t>(len));
  }

  applyWireTimeout(wire, timeoutMs);

  wire->beginTransmission(addr);
  size_t written = wire->write(data, len);
  if (written != len) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "I2C write incomplete",
                                  static_cast<int32_t>(written));
  }

  return mapWireStatus(wire->endTransmission(true));  // Send STOP
}

/**
 * @brief Wire-based I2C write-read implementation.
 *
 * Pass to Config::i2cWriteRead, and pass &Wire (or custom TwoWire*) to i2cUser.
 *
 * @param addr I2C 7-bit address
 * @param tx TX buffer to send
 * @param txLen TX length
 * @param rx RX buffer for readback
 * @param rxLen RX length
 * @param timeoutMs Timeout requested by the driver (bus-manager owned in shared buses)
 * @param user Pointer to TwoWire instance
 * @return Status OK on success, I2C error on failure
 */
inline ADS1115::Status wireWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                                     uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                     void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  if (wire == nullptr) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "Wire instance is null");
  }
  if ((txLen > 0 && tx == nullptr) || (rxLen > 0 && rx == nullptr)) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "Invalid I2C read params");
  }
  if (txLen == 0 || rxLen == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "I2C read length invalid");
  }
  if (txLen > 128 || rxLen > 128) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM, "I2C read exceeds buffer");
  }

  applyWireTimeout(wire, timeoutMs);

  wire->beginTransmission(addr);
  size_t written = wire->write(tx, txLen);
  if (written != txLen) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "I2C write incomplete",
                                  static_cast<int32_t>(written));
  }

  const ADS1115::Status writeStatus = mapWireStatus(wire->endTransmission(false));
  if (!writeStatus.ok()) {
    return writeStatus;
  }

  size_t read = wire->requestFrom(addr, static_cast<uint8_t>(rxLen));
  if (read != rxLen) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "I2C read length mismatch",
                                  static_cast<int32_t>(read));
  }

  for (size_t i = 0; i < rxLen; ++i) {
    if (wire->available()) {
      rx[i] = static_cast<uint8_t>(wire->read());
    } else {
      return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "I2C data not available");
    }
  }

  return ADS1115::Status::Ok();
}

/**
 * @brief Arduino millisecond timestamp callback for Config::nowMs.
 */
inline uint32_t arduinoNowMs(void*) {
  return millis();
}

/**
 * @brief Arduino scheduler hint callback for Config::cooperativeYield.
 */
inline void arduinoYield(void*) {
  yield();
}

/**
 * @brief Initialize Wire with default pins and frequency.
 *
 * @param sda SDA pin number
 * @param scl SCL pin number
 * @param freq I2C clock frequency in Hz (default 400kHz)
 * @param timeoutMs I2C timeout in milliseconds (default 50ms)
 * @return true on success
 */
inline bool initWire(int sda, int scl, uint32_t freq = 400000,
                     uint16_t timeoutMs = 50) {
#if defined(ARDUINO_ARCH_ESP32)
  // Application-owned bus clear (UM10204 3.1.16). Release, never drive HIGH
  // against a target holding SDA or stretching SCL. One deadline bounds all
  // clock waits; an uncleared bus must not be reported as initialized.
  const uint32_t startedUs = micros();
  const uint32_t timeoutUs = static_cast<uint32_t>(timeoutMs) * 1000U;
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, HIGH);
  const auto releaseClock = [&]() {
    digitalWrite(scl, HIGH);
    while (digitalRead(scl) == LOW) {
      if (static_cast<uint32_t>(micros() - startedUs) >= timeoutUs) {
        digitalWrite(sda, HIGH);
        return false;
      }
      delay(1);
    }
    if (static_cast<uint32_t>(micros() - startedUs) >= timeoutUs) {
      digitalWrite(sda, HIGH);
      return false;
    }
    return true;
  };
  if (!releaseClock()) return false;
  for (int i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(5);
    if (!releaseClock()) return false;
    delayMicroseconds(5);
  }
  digitalWrite(scl, LOW);
  digitalWrite(sda, LOW);
  delayMicroseconds(5);
  if (!releaseClock()) return false;
  delayMicroseconds(5);
  digitalWrite(sda, HIGH);
  delayMicroseconds(5);
  if (digitalRead(sda) == LOW || digitalRead(scl) == LOW) return false;

#endif

#if defined(ARDUINO_ARCH_ESP32)
  // Set the desired clock during initialization. Arduino-ESP32 3.3.11's
  // separate setClock() reports failure before any device handle exists.
  if (!Wire.begin(sda, scl, freq)) {
    return false;
  }
#else
  if (!Wire.begin(sda, scl)) {
    return false;
  }
  if (!Wire.setClock(freq)) {
    return false;
  }
#endif
#if defined(ARDUINO_ARCH_ESP32)
  Wire.setTimeOut(timeoutMs);
#else
  (void)timeoutMs;
#endif
  return true;
}

}  // namespace transport
