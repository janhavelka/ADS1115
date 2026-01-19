/// @file Config.h
/// @brief Configuration structure for ADS1115 driver
#pragma once

#include <cstddef>
#include <cstdint>
#include "ADS1115/Status.h"

namespace ADS1115 {

/// I2C write callback signature
/// @param addr     I2C device address (7-bit)
/// @param data     Pointer to data to write
/// @param len      Number of bytes to write
/// @param timeoutMs Maximum time to wait for completion
/// @param user     User context pointer passed through from Config
/// @return Status indicating success or failure
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// I2C write-then-read callback signature
/// @param addr     I2C device address (7-bit)
/// @param txData   Pointer to data to write
/// @param txLen    Number of bytes to write
/// @param rxData   Pointer to buffer for read data
/// @param rxLen    Number of bytes to read
/// @param timeoutMs Maximum time to wait for completion
/// @param user     User context pointer passed through from Config
/// @return Status indicating success or failure
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* txData, size_t txLen,
                                  uint8_t* rxData, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// Input multiplexer configuration
enum class Mux : uint8_t {
  AIN0_AIN1 = 0,  ///< Differential: AIN0 - AIN1 (default)
  AIN0_AIN3 = 1,  ///< Differential: AIN0 - AIN3
  AIN1_AIN3 = 2,  ///< Differential: AIN1 - AIN3
  AIN2_AIN3 = 3,  ///< Differential: AIN2 - AIN3
  AIN0_GND  = 4,  ///< Single-ended: AIN0
  AIN1_GND  = 5,  ///< Single-ended: AIN1
  AIN2_GND  = 6,  ///< Single-ended: AIN2
  AIN3_GND  = 7   ///< Single-ended: AIN3
};

/// Programmable Gain Amplifier (full-scale range)
enum class Gain : uint8_t {
  FSR_6_144V = 0,  ///< +/-6.144V (LSB = 187.5uV)
  FSR_4_096V = 1,  ///< +/-4.096V (LSB = 125uV)
  FSR_2_048V = 2,  ///< +/-2.048V (LSB = 62.5uV) - default
  FSR_1_024V = 3,  ///< +/-1.024V (LSB = 31.25uV)
  FSR_0_512V = 4,  ///< +/-0.512V (LSB = 15.625uV)
  FSR_0_256V = 5   ///< +/-0.256V (LSB = 7.8125uV)
};

/// Data rate (samples per second)
enum class DataRate : uint8_t {
  SPS_8   = 0,   ///<   8 SPS
  SPS_16  = 1,   ///<  16 SPS
  SPS_32  = 2,   ///<  32 SPS
  SPS_64  = 3,   ///<  64 SPS
  SPS_128 = 4,   ///< 128 SPS (default)
  SPS_250 = 5,   ///< 250 SPS
  SPS_475 = 6,   ///< 475 SPS
  SPS_860 = 7    ///< 860 SPS
};

/// Operating mode
enum class Mode : uint8_t {
  CONTINUOUS  = 0,  ///< Continuous conversion mode
  SINGLE_SHOT = 1   ///< Single-shot / power-down mode (default)
};

/// Comparator mode
enum class ComparatorMode : uint8_t {
  TRADITIONAL = 0,  ///< Traditional comparator (default)
  WINDOW      = 1   ///< Window comparator
};

/// Comparator polarity
enum class ComparatorPolarity : uint8_t {
  ACTIVE_LOW  = 0,  ///< ALERT/RDY active low (default)
  ACTIVE_HIGH = 1   ///< ALERT/RDY active high
};

/// Comparator latch
enum class ComparatorLatch : uint8_t {
  NON_LATCHING = 0,  ///< Non-latching (default)
  LATCHING     = 1   ///< Latching
};

/// Comparator queue (assertions before ALERT)
enum class ComparatorQueue : uint8_t {
  ASSERT_1 = 0,  ///< Assert after 1 conversion
  ASSERT_2 = 1,  ///< Assert after 2 conversions
  ASSERT_4 = 2,  ///< Assert after 4 conversions
  DISABLE  = 3   ///< Disable comparator (default), ALERT/RDY high-Z
};

/// Configuration for ADS1115 driver
struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;

  // === Device Settings ===
  uint8_t i2cAddress = 0x48;       ///< 0x48-0x4B based on ADDR pin
  uint32_t i2cTimeoutMs = 50;      ///< I2C transaction timeout in ms

  // === Conversion Settings ===
  Mux mux = Mux::AIN0_GND;               ///< Input multiplexer
  Gain gain = Gain::FSR_2_048V;          ///< PGA gain
  DataRate dataRate = DataRate::SPS_128; ///< Data rate
  Mode mode = Mode::SINGLE_SHOT;         ///< Operating mode

  // === Comparator Settings (optional) ===
  ComparatorMode compMode = ComparatorMode::TRADITIONAL;
  ComparatorPolarity compPolarity = ComparatorPolarity::ACTIVE_LOW;
  ComparatorLatch compLatch = ComparatorLatch::NON_LATCHING;
  ComparatorQueue compQueue = ComparatorQueue::DISABLE;
  int16_t compThresholdHigh = 0x7FFF;  ///< High threshold (default: max)
  int16_t compThresholdLow = 0x8000;   ///< Low threshold (default: min)

  // === Health Tracking ===
  uint8_t offlineThreshold = 5;    ///< Consecutive failures before OFFLINE
};

} // namespace ADS1115
