/// @file ADS1115.h
/// @brief Main driver class for ADS1115
#pragma once

#include <cstddef>
#include <cstdint>

#include "ADS1115/CommandTable.h"
#include "ADS1115/Config.h"
#include "ADS1115/Status.h"
#include "ADS1115/Version.h"

namespace ADS1115 {

/// Driver state for health monitoring
enum class DriverState : uint8_t {
  UNINIT,    ///< begin() not called or end() called
  READY,     ///< Operational, consecutiveFailures == 0
  DEGRADED,  ///< 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    ///< consecutiveFailures >= offlineThreshold
};

/// Snapshot of driver configuration and runtime state without I2C access.
struct SettingsSnapshot {
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  uint8_t i2cAddress = 0x48;
  uint32_t i2cTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  bool hasNowMsHook = false;
  bool hasGpioReadHook = false;
  bool hasCooperativeYieldHook = false;
  int alertRdyPin = -1;
  bool alertRdyPinConfigured = false;
  bool conversionReadyModeEnabled = false;
  bool usesAlertRdyPin = false;
  Mux mux = Mux::AIN0_GND;
  Gain gain = Gain::FSR_2_048V;
  DataRate dataRate = DataRate::SPS_128;
  Mode mode = Mode::SINGLE_SHOT;
  ComparatorMode compMode = ComparatorMode::TRADITIONAL;
  ComparatorPolarity compPolarity = ComparatorPolarity::ACTIVE_LOW;
  ComparatorLatch compLatch = ComparatorLatch::NON_LATCHING;
  ComparatorQueue compQueue = ComparatorQueue::DISABLE;
  int16_t compThresholdHigh = 0x7FFF;
  int16_t compThresholdLow = static_cast<int16_t>(0x8000);
  bool conversionStarted = false;
  bool conversionReady = false;
  uint32_t conversionStartMs = 0;
  int16_t lastRawValue = 0;
};

/// ADS1115 driver class
class ADS1115 {
public:
  // === Lifecycle ===
  /// Initialize the driver with configuration and verify device presence.
  Status begin(const Config& config);
  /// Process pending operations (currently bounded polling only).
  void tick(uint32_t nowMs);
  /// Shut the device down to single-shot idle and clear cached conversion state.
  void end();

  /// Check if begin() completed successfully and end() has not been called.
  bool isInitialized() const { return _initialized; }

  /// Get the cached configuration snapshot currently owned by the driver.
  const Config& getConfig() const { return _config; }

  // === Diagnostics (no health tracking) ===
  Status probe();
  Status recover();

  /// Populate a snapshot of cached configuration and runtime state without I2C.
  /// @param[out] out Snapshot to fill
  /// @return Status::Ok() always
  Status getSettings(SettingsSnapshot& out) const;

  // === Driver State ===
  DriverState state() const { return _driverState; }
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  // === Health Tracking ===
  uint32_t lastOkMs() const { return _lastOkMs; }
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  Status lastError() const { return _lastError; }
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  uint32_t totalFailures() const { return _totalFailures; }
  uint32_t totalSuccess() const { return _totalSuccess; }

  // === Conversion API ===
  /// Start one single-shot conversion using the cached mux.
  /// @return Err::IN_PROGRESS when the conversion was started.
  Status startConversion();

  /// Start one single-shot conversion after applying the requested mux.
  /// The cached mux is restored if the CONFIG write fails.
  /// @return Err::IN_PROGRESS when the conversion was started.
  Status startConversion(Mux mux);

  /// Convenience wrapper around readConversionReady(). Returns false when the
  /// driver is not initialized or when the underlying CONFIG read fails.
  bool conversionReady();

  /// Check conversion readiness with explicit error reporting.
  /// Uses ALERT/RDY when configured for conversion-ready mode. Otherwise,
  /// single-shot mode polls the OS bit after the conversion time and continuous
  /// mode tracks the configured data-rate interval between fresh samples.
  /// @param[out] ready true when conversion data can be read
  /// @return Status from the readiness path
  Status readConversionReady(bool& ready);

  /// Read the conversion register as a signed two's-complement sample.
  Status readRaw(int16_t& out);

  /// Read a signed sample and scale it using the active gain.
  Status readVoltage(float& volts);

  /// Start or join a single-shot conversion and wait with a finite deadline.
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);

  /// Blocking read with voltage scaling.
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);

  // === Configuration ===
  /// Set the input multiplexer. Cache changes commit only after I2C success.
  Status setMux(Mux mux);
  Mux getMux() const { return _config.mux; }

  /// Set PGA full-scale range. Cache changes commit only after I2C success.
  Status setGain(Gain gain);
  Gain getGain() const { return _config.gain; }

  /// Set output data rate. Cache changes commit only after I2C success.
  Status setDataRate(DataRate rate);
  DataRate getDataRate() const { return _config.dataRate; }

  /// Set operating mode. Cache changes commit only after I2C success.
  Status setMode(Mode mode);
  Mode getMode() const { return _config.mode; }

  /// Read the ADS1115 CONFIG register.
  Status readConfig(uint16_t& config);

  /// Write a validated CONFIG register value and sync the typed cache.
  Status writeConfig(uint16_t config);

  // === Raw Register Access ===
  /// Read a 16-bit register using tracked I2C access.
  /// Valid register pointers are 0x00..0x03.
  Status readRegister16(uint8_t reg, uint16_t& value);

  /// Write a 16-bit register using tracked I2C access.
  /// Valid register pointers are 0x00..0x03.
  Status writeRegister16(uint8_t reg, uint16_t value);

  /// Compatibility alias for readRegister16().
  Status readRegister(uint8_t reg, uint16_t& value) { return readRegister16(reg, value); }

  /// Compatibility alias for writeRegister16().
  Status writeRegister(uint8_t reg, uint16_t value) { return writeRegister16(reg, value); }

  // === Comparator ===
  /// Set signed comparator thresholds. Cache changes commit after both writes succeed.
  Status setThresholds(int16_t low, int16_t high);

  /// Read signed comparator thresholds and sync the cache.
  Status getThresholds(int16_t& low, int16_t& high);

  /// Set comparator mode. Cache changes commit only after I2C success.
  Status setComparatorMode(ComparatorMode mode);
  ComparatorMode getComparatorMode() const { return _config.compMode; }

  /// Set ALERT/RDY polarity. Cache changes commit only after I2C success.
  Status setComparatorPolarity(ComparatorPolarity polarity);
  ComparatorPolarity getComparatorPolarity() const { return _config.compPolarity; }

  /// Set comparator latch behavior. Cache changes commit only after I2C success.
  Status setComparatorLatch(ComparatorLatch latch);
  ComparatorLatch getComparatorLatch() const { return _config.compLatch; }

  /// Set comparator queue depth or disable comparator.
  Status setComparatorQueue(ComparatorQueue queue);
  ComparatorQueue getComparatorQueue() const { return _config.compQueue; }

  bool isAlertRdyPinConfigured() const;
  bool isConversionReadyModeEnabled() const;
  bool usesAlertRdyPinForConversionReady() const;

  /// Program ADS1115 threshold/comparator fields for conversion-ready ALERT/RDY mode.
  Status enableConversionReadyPin();

  /// Disable comparator output by setting queue to DISABLE.
  Status disableComparator();

  // === Utility ===
  float rawToVoltage(int16_t raw) const;
  float getLsbVoltage() const;
  uint32_t getConversionTimeMs() const;

private:
  class ScopedOfflineI2cAllowance;

  // === Transport Wrappers ===
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                          uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                              uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteTracked(const uint8_t* buf, size_t len);

  // === Register Access ===
  Status _readRegister16Raw(uint8_t reg, uint16_t& value);
  Status _readRegister16Tracked(uint8_t reg, uint16_t& value);
  Status _writeRegister16Tracked(uint8_t reg, uint16_t value);

  // === Health Tracking ===
  Status _updateHealth(const Status& st);
  void _reassertOfflineLatch();

  // === Internal ===
  Status _readConversionReadyAt(uint32_t nowMs, bool& ready);
  Status _applyConfig();
  uint16_t _buildConfigRegister() const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;

  // === State ===
  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;
  bool _allowOfflineI2c = false;

  // === Health Counters ===
  uint32_t _lastOkMs = 0;
  uint32_t _lastErrorMs = 0;
  Status _lastError = Status::Ok();
  uint8_t _consecutiveFailures = 0;
  uint32_t _totalFailures = 0;
  uint32_t _totalSuccess = 0;

  // === Conversion State ===
  bool _conversionStarted = false;
  bool _conversionReady = false;
  uint32_t _conversionStartMs = 0;
  int16_t _lastRawValue = 0;
};

} // namespace ADS1115
