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

/// GPIO read callback signature (for ALERT/RDY pin)
/// @param pin      GPIO pin number
/// @param user     User context pointer passed through from Config
/// @return true if pin level is HIGH, false if LOW
using GpioReadFn = bool (*)(int pin, void* user);

/// Millisecond timestamp callback.
/// @param user User context pointer passed through from Config
/// @return Current monotonic milliseconds
using NowMsFn = uint32_t (*)(void* user);

/// Cooperative yield callback.
/// @param user User context pointer passed through from Config
using YieldFn = void (*)(void* user);

/// @brief Input multiplexer configuration.
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

/// @brief Programmable gain amplifier full-scale range.
///
/// These values set ADC full-scale conversion range only. They do not increase
/// ADS1115 analog input absolute maximum ratings; keep all inputs within the
/// datasheet electrical limits for the powered device.
enum class Gain : uint8_t {
  FSR_6_144V = 0,  ///< +/-6.144V (LSB = 187.5uV)
  FSR_4_096V = 1,  ///< +/-4.096V (LSB = 125uV)
  FSR_2_048V = 2,  ///< +/-2.048V (LSB = 62.5uV) - default
  FSR_1_024V = 3,  ///< +/-1.024V (LSB = 31.25uV)
  FSR_0_512V = 4,  ///< +/-0.512V (LSB = 15.625uV)
  FSR_0_256V = 5   ///< +/-0.256V (LSB = 7.8125uV)
};

/// @brief Output data rate in samples per second.
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

/// @brief ADS1115 operating mode.
enum class Mode : uint8_t {
  CONTINUOUS  = 0,  ///< Continuous conversion mode
  SINGLE_SHOT = 1   ///< Single-shot / power-down mode (default)
};

/// @brief Comparator mode.
enum class ComparatorMode : uint8_t {
  TRADITIONAL = 0,  ///< Traditional comparator (default)
  WINDOW      = 1   ///< Window comparator
};

/// @brief ALERT/RDY comparator polarity.
enum class ComparatorPolarity : uint8_t {
  ACTIVE_LOW  = 0,  ///< ALERT/RDY active low (default)
  ACTIVE_HIGH = 1   ///< ALERT/RDY active high
};

/// @brief Comparator latch behavior.
enum class ComparatorLatch : uint8_t {
  NON_LATCHING = 0,  ///< Non-latching (default)
  LATCHING     = 1   ///< Latching
};

/// @brief Comparator queue depth before ALERT assertion.
enum class ComparatorQueue : uint8_t {
  ASSERT_1 = 0,  ///< Assert after 1 conversion
  ASSERT_2 = 1,  ///< Assert after 2 conversions
  ASSERT_4 = 2,  ///< Assert after 4 conversions
  DISABLE  = 3   ///< Disable comparator (default), ALERT/RDY high-Z
};

/// @brief Comparator use selected by an owner-safe device profile.
enum class ComparatorUse : uint8_t {
  OFF = 0,              ///< Comparator output disabled/high impedance
  THRESHOLD,            ///< Traditional or window threshold comparator
  CONVERSION_READY      ///< Datasheet conversion-ready threshold pattern
};

/// @brief Complete comparator configuration for atomic profile validation.
struct ComparatorProfile {
  ComparatorUse use = ComparatorUse::OFF;
  ComparatorMode mode = ComparatorMode::TRADITIONAL;
  ComparatorPolarity polarity = ComparatorPolarity::ACTIVE_LOW;
  ComparatorLatch latch = ComparatorLatch::NON_LATCHING;
  ComparatorQueue queue = ComparatorQueue::DISABLE;
  int16_t lowThreshold = static_cast<int16_t>(0x8000);
  int16_t highThreshold = 0x7FFF;
};

/// @brief Non-owning transport binding used by the owner-safe API.
///
/// Bus handles, pins, locking, clock rate, retries, recovery, and scheduling
/// remain owned by the application. Each callback must enforce transferTimeoutMs.
struct DriverConfig {
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;
  uint32_t transferTimeoutMs = 50; ///< Per-callback cap; poll clamps to deadline remaining
};

/// @brief Complete desired hardware register profile.
struct DeviceProfile {
  uint8_t i2cAddress = 0x48;
  Mux defaultMux = Mux::AIN0_GND;
  Gain defaultGain = Gain::FSR_2_048V;
  DataRate dataRate = DataRate::SPS_128;
  Mode mode = Mode::SINGLE_SHOT;
  ComparatorProfile comparator{};
};

/// @brief One typed single-shot channel request.
struct ChannelRequest {
  uint16_t channelId = 0;
  Mux mux = Mux::AIN0_GND;
  Gain gain = Gain::FSR_2_048V;
};

/// @name Pure owner helpers
/// These functions perform no I2C, allocation, logging, or framework calls.
/// @{
Status validateDeviceProfile(const DeviceProfile& profile);
Status validateChannelRequest(const ChannelRequest& request);
Status validateComparatorProfile(const ComparatorProfile& profile);
uint16_t dataRateSps(DataRate rate); ///< Returns zero for an invalid rate
/// Worst conversion interval including -10% minimum SPS tolerance and 1 ms guard.
uint32_t worstCaseConversionTimeUs(DataRate rate);
int32_t gainFullScaleMicrovolts(Gain gain); ///< Returns zero for an invalid gain
/// Convert a signed code to rounded ADC-input microvolts using int64 arithmetic.
Status rawToMicrovolts(int16_t raw, Gain gain, int32_t& out);
bool isSingleEnded(Mux mux);
int8_t positiveInput(Mux mux); ///< AIN index, or -1 for invalid MUX
int8_t negativeInput(Mux mux); ///< AIN index, -1 for GND, or -2 for invalid MUX
/// Derive a bounded duration for channelCount conversions plus caller margin.
uint32_t operationDeadlineMs(uint8_t channelCount, DataRate rate,
                             uint32_t schedulingMarginMs);
/// @}

/// @brief Configuration for ADS1115 driver.
struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;

  // === Timing Hooks (optional; required by blocking conversion APIs) ===
  /// Monotonic source. Required by readBlocking* and by direct timing-based
  /// readiness checks that should advance without tick(nowMs)/service(nowMs).
  /// Without this hook, health timestamps are unavailable and report as 0;
  /// ALERT/RDY readiness still needs an external tick/service timebase to pass
  /// the conversion interval before the GPIO path is evaluated.
  NowMsFn nowMs = nullptr;
  YieldFn cooperativeYield = nullptr;      ///< Cooperative scheduler hint
  void* timeUser = nullptr;                ///< User context for timing hooks

  // === Device Settings ===
  uint8_t i2cAddress = 0x48;       ///< 0x48-0x4B based on ADDR pin
  uint32_t i2cTimeoutMs = 50;      ///< I2C transaction timeout in ms
  bool strictInitVerify = true;    ///< Compatibility field; production init always verifies

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
  int16_t compThresholdLow = -32768;   ///< Low threshold (default: min, 0x8000)

  // === ALERT/RDY Pin (optional) ===
  int alertRdyPin = -1;        ///< GPIO pin for ALERT/RDY; -1 means not used
  GpioReadFn gpioRead = nullptr;
  void* gpioUser = nullptr;

  // === Health Tracking ===
  uint8_t offlineThreshold = 5;    ///< Passive diagnostic threshold; never gates owner I/O
};

} // namespace ADS1115
