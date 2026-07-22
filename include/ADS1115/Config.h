/// @file Config.h
/// @brief Configuration structure for ADS1115 driver
#pragma once

#include <cstddef>
#include <cstdint>
#include "ADS1115/Status.h"

namespace ADS1115 {

/// I2C write callback signature.
///
/// Perform exactly one externally serialized physical transfer attempt. Do not
/// hide retry or bus recovery in the callback. The data buffer is valid only
/// for this call, and lock acquisition plus transfer must honor timeoutMs.
/// @param addr     I2C device address (7-bit)
/// @param data     Pointer to data to write
/// @param len      Number of bytes to write
/// @param timeoutMs Maximum time to wait for completion
/// @param user     User context pointer passed through from Config
/// @return Meaningful library Status; preserve transport-native detail and do
///         not report definite address/data NACK unless the phase is proven.
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// I2C write-then-read callback signature.
///
/// Perform exactly one externally serialized write/repeated-start/read attempt.
/// Do not hide retry or recovery. Buffers are valid only for this call, and
/// lock acquisition plus the complete transfer must honor timeoutMs.
/// @param addr     I2C device address (7-bit)
/// @param txData   Pointer to data to write
/// @param txLen    Number of bytes to write
/// @param rxData   Pointer to buffer for read data
/// @param rxLen    Number of bytes to read
/// @param timeoutMs Maximum time to wait for completion
/// @param user     User context pointer passed through from Config
/// @return Meaningful library Status; preserve transport-native detail and do
///         not report definite address/data NACK unless the phase is proven.
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
  ComparatorUse use = ComparatorUse::OFF; ///< Output purpose and threshold encoding
  ComparatorMode mode = ComparatorMode::TRADITIONAL; ///< Threshold/window behavior
  ComparatorPolarity polarity = ComparatorPolarity::ACTIVE_LOW; ///< Active output level
  ComparatorLatch latch = ComparatorLatch::NON_LATCHING; ///< Latching behavior
  ComparatorQueue queue = ComparatorQueue::DISABLE; ///< Assertion queue depth
  int16_t lowThreshold = static_cast<int16_t>(0x8000); ///< Signed raw low threshold
  int16_t highThreshold = 0x7FFF; ///< Signed raw high threshold
};

/// @brief Non-owning transport binding used by the owner-safe API.
///
/// Bus handles, pins, locking, clock rate, retries, recovery, and scheduling
/// remain owned by the application. Each callback must enforce transferTimeoutMs.
/// The context and callback targets must outlive the binding. Calls require
/// externally serialized task context and are not ISR-safe.
struct DriverConfig {
  I2cWriteFn i2cWrite = nullptr; ///< Required application-owned write callback
  I2cWriteReadFn i2cWriteRead = nullptr; ///< Required repeated-start read callback
  void* i2cUser = nullptr; ///< Opaque application transport context
  uint32_t transferTimeoutMs = 50; ///< Per-callback cap; poll clamps to deadline remaining
};

/// @brief Complete desired hardware register profile.
struct DeviceProfile {
  uint8_t i2cAddress = 0x48; ///< Seven-bit address in the legal 0x48-0x4B range
  Mux defaultMux = Mux::AIN0_GND; ///< MUX restored by profile apply/recovery
  Gain defaultGain = Gain::FSR_2_048V; ///< PGA restored by profile apply/recovery
  DataRate dataRate = DataRate::SPS_128; ///< Conversion data rate
  Mode mode = Mode::SINGLE_SHOT; ///< Conversion mode; typed reads require single-shot
  ComparatorProfile comparator{}; ///< Complete comparator/ALERT profile
};

/// @brief One typed single-shot channel request.
struct ChannelRequest {
  uint16_t channelId = 0; ///< Application-owned identity copied into SampleResult
  Mux mux = Mux::AIN0_GND; ///< Input selection for this conversion
  Gain gain = Gain::FSR_2_048V; ///< PGA range for this conversion
};

/// @name Pure owner helpers
/// These functions perform no I2C, allocation, logging, or framework calls.
/// @{

/// @brief Validate a complete desired device profile without I2C.
/// @param profile Candidate device profile.
/// @return OK when every field and cross-field comparator rule is valid.
Status validateDeviceProfile(const DeviceProfile& profile);
/// @param request Candidate typed channel request.
/// @return OK when the MUX and PGA values are valid.
Status validateChannelRequest(const ChannelRequest& request);
/// @param profile Candidate comparator profile.
/// @return OK when use, queue, and threshold fields form a valid profile.
Status validateComparatorProfile(const ComparatorProfile& profile);
/// @param rate Data-rate enum value.
/// @return Nominal samples per second, or zero for an invalid value.
uint16_t dataRateSps(DataRate rate);
/// Worst conversion interval including -10% minimum SPS tolerance and 1 ms guard.
/// @param rate Data-rate enum value.
/// @return Bounded interval in microseconds, or zero for an invalid value.
uint32_t worstCaseConversionTimeUs(DataRate rate);
/// @param gain PGA range.
/// @return Positive full-scale magnitude in microvolts, or zero if invalid.
int32_t gainFullScaleMicrovolts(Gain gain);
/// Convert a signed code to rounded ADC-input microvolts using int64 arithmetic.
/// @param raw Signed ADC conversion code.
/// @param gain PGA range used for the conversion.
/// @param[out] out Rounded ADC-input microvolts, or zero on invalid gain.
/// @return OK, or INVALID_PARAM for an invalid gain.
Status rawToMicrovolts(int16_t raw, Gain gain, int32_t& out);
/// @param mux Input multiplexer selection.
/// @return true only for a valid AINx-to-GND selection.
bool isSingleEnded(Mux mux);
/// @param mux Input multiplexer selection.
/// @return Positive AIN index, or -1 for an invalid MUX.
int8_t positiveInput(Mux mux);
/// @param mux Input multiplexer selection.
/// @return Negative AIN index, -1 for GND, or -2 for an invalid MUX.
int8_t negativeInput(Mux mux);
/// Derive a bounded duration for channelCount conversions plus caller margin.
/// @param channelCount Number of sequential conversions.
/// @param rate Conversion data rate.
/// @param schedulingMarginMs Caller-owned callback and scheduling allowance.
/// @return Duration in milliseconds, saturated at INT32_MAX; zero when the
///         channel count or data rate is invalid.
uint32_t operationDeadlineMs(uint8_t channelCount, DataRate rate,
                             uint32_t schedulingMarginMs);
/// @}

/// @brief Compatibility configuration for the synchronous diagnostic surface.
///
/// Callback/context lifetime, external serialization, timeout enforcement, and
/// non-ISR task context remain application responsibilities.
struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr; ///< Required application-owned write callback
  I2cWriteReadFn i2cWriteRead = nullptr; ///< Required repeated-start read callback
  void* i2cUser = nullptr; ///< Opaque application transport context

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
  ComparatorMode compMode = ComparatorMode::TRADITIONAL; ///< Threshold/window mode
  ComparatorPolarity compPolarity = ComparatorPolarity::ACTIVE_LOW; ///< Active level
  ComparatorLatch compLatch = ComparatorLatch::NON_LATCHING; ///< Latch policy
  ComparatorQueue compQueue = ComparatorQueue::DISABLE; ///< Queue depth or disabled
  int16_t compThresholdHigh = 0x7FFF;  ///< High threshold (default: max)
  int16_t compThresholdLow = -32768;   ///< Low threshold (default: min, 0x8000)

  // === ALERT/RDY Pin (optional) ===
  int alertRdyPin = -1;        ///< GPIO pin for ALERT/RDY; -1 means not used
  GpioReadFn gpioRead = nullptr; ///< Application-owned GPIO level callback
  void* gpioUser = nullptr; ///< Opaque GPIO callback context

  // === Health Tracking ===
  uint8_t offlineThreshold = 5;    ///< Passive diagnostic threshold; never gates owner I/O
};

} // namespace ADS1115
