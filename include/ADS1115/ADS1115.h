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

/// State of the optional poll-chunked job executor.
enum class JobState : uint8_t {
  IDLE,                         ///< No job has been scheduled.
  SINGLE_SHOT_WRITE_CONFIG,     ///< Next instruction writes config with OS/start bit.
  SINGLE_SHOT_WAIT_CONVERSION,  ///< Waiting for conversion time or ALERT/RDY.
  SINGLE_SHOT_POLL_READY,       ///< Next instruction reads config OS/ready bit.
  SINGLE_SHOT_READ_CONVERSION,  ///< Next instruction reads conversion register.
  APPLY_WRITE_LOW_THRESHOLD,    ///< Next instruction writes low threshold.
  APPLY_WRITE_HIGH_THRESHOLD,   ///< Next instruction writes high threshold.
  APPLY_WRITE_CONFIG,           ///< Next instruction writes config register.
  COMPLETE,                     ///< Last job completed successfully.
  FAILED                        ///< Last job failed; lastJobStatus() has details.
};

/// Result returned by poll-chunked job calls.
struct PollResult {
  Status status = Status::Ok();       ///< Current or terminal job status.
  uint8_t instructionsUsed = 0;       ///< Transport callbacks used by this poll call.
  bool done = true;                   ///< True when the job is no longer active.
  JobState state = JobState::IDLE;    ///< State after this poll call.
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
  bool hardwareConfigDirty = false;
  bool hardwareConfigUncertain = false;
  Status lastConfigApplyError = Status::Ok();
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
  bool isHardwareConfigDirty() const { return _hardwareConfigDirty; }
  bool isHardwareConfigUncertain() const { return _hardwareConfigUncertain; }
  Status lastConfigApplyError() const { return _lastConfigApplyError; }

  // === Conversion API ===
  Status startConversion();
  Status startConversion(Mux mux);
  bool conversionReady();
  Status readRaw(int16_t& out);
  Status readVoltage(float& volts);
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);

  /// Start a poll-chunked single-shot conversion job without performing I2C.
  Status startSingleShot();
  /// Start a poll-chunked single-shot conversion job for a mux without I2C.
  Status startSingleShot(Mux mux);
  /// Advance a single-shot job by at most maxInstructions transport callbacks.
  PollResult pollSingleShot(uint32_t nowMs, uint8_t maxInstructions = 1);

  /// Start a staged config-apply job using the currently cached config.
  Status startApplyConfigJob();
  /// Advance a config-apply job by at most maxInstructions transport callbacks.
  PollResult pollApplyConfig(uint32_t nowMs, uint8_t maxInstructions = 1);

  /// Cancel the active poll-chunked job without touching hardware.
  void cancelJob();
  bool jobActive() const { return _jobActive; }
  JobState jobState() const { return _jobState; }
  Status lastJobStatus() const { return _lastJobStatus; }
  int16_t lastRawValue() const { return _lastRawValue; }

  // === Configuration ===
  Status setMux(Mux mux);
  Mux getMux() const { return _config.mux; }

  Status setGain(Gain gain);
  Gain getGain() const { return _config.gain; }

  Status setDataRate(DataRate rate);
  DataRate getDataRate() const { return _config.dataRate; }

  Status setMode(Mode mode);
  Mode getMode() const { return _config.mode; }

  Status readConfig(uint16_t& config);
  Status writeConfig(uint16_t config);

  // === Raw Register Access ===
  /// Read a 16-bit register using tracked I2C access.
  Status readRegister16(uint8_t reg, uint16_t& value);

  /// Write a 16-bit register using tracked I2C access.
  Status writeRegister16(uint8_t reg, uint16_t value);

  /// Compatibility alias for readRegister16().
  Status readRegister(uint8_t reg, uint16_t& value) { return readRegister16(reg, value); }

  /// Compatibility alias for writeRegister16().
  Status writeRegister(uint8_t reg, uint16_t value) { return writeRegister16(reg, value); }

  // === Comparator ===
  Status setThresholds(int16_t low, int16_t high);
  Status getThresholds(int16_t& low, int16_t& high);

  Status setComparatorMode(ComparatorMode mode);
  ComparatorMode getComparatorMode() const { return _config.compMode; }

  Status setComparatorPolarity(ComparatorPolarity polarity);
  ComparatorPolarity getComparatorPolarity() const { return _config.compPolarity; }

  Status setComparatorLatch(ComparatorLatch latch);
  ComparatorLatch getComparatorLatch() const { return _config.compLatch; }

  Status setComparatorQueue(ComparatorQueue queue);
  ComparatorQueue getComparatorQueue() const { return _config.compQueue; }

  bool isAlertRdyPinConfigured() const;
  bool isConversionReadyModeEnabled() const;
  bool usesAlertRdyPinForConversionReady() const;

  Status enableConversionReadyPin();
  Status disableComparator();

  // === Utility ===
  float rawToVoltage(int16_t raw) const;
  float getLsbVoltage() const;
  uint32_t getConversionTimeMs() const;

private:
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

  // === Internal ===
  Status _applyConfig();
  uint16_t _buildConfigRegister() const;
  uint16_t _buildConfigRegisterForMux(Mux mux) const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;
  uint8_t _instructionBudget(uint8_t maxInstructions) const;
  PollResult _pollResult(Status status, uint8_t instructionsUsed, bool done) const;
  PollResult _finishJob(uint8_t instructionsUsed);
  PollResult _failJob(const Status& status, uint8_t instructionsUsed);
  bool _isOffline() const;
  Status _offlineStatus() const;
  void _markHardwareConfigUncertain(const Status& st);
  void _markHardwareConfigClean();

  // === State ===
  static constexpr uint8_t MAX_JOB_INSTRUCTIONS = 3;

  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;

  // === Poll-Chunked Job State ===
  bool _jobActive = false;
  JobState _jobState = JobState::IDLE;
  Status _lastJobStatus = Status::Ok();
  uint16_t _jobConfigRegister = 0;
  int16_t _jobThresholdLow = 0;
  int16_t _jobThresholdHigh = 0;
  bool _jobMuxOverride = false;
  Mux _jobMux = Mux::AIN0_GND;

  // === Health Counters ===
  uint32_t _lastOkMs = 0;
  uint32_t _lastErrorMs = 0;
  Status _lastError = Status::Ok();
  uint8_t _consecutiveFailures = 0;
  uint32_t _totalFailures = 0;
  uint32_t _totalSuccess = 0;
  bool _hardwareConfigDirty = false;
  bool _hardwareConfigUncertain = false;
  Status _lastConfigApplyError = Status::Ok();

  // === Conversion State ===
  bool _conversionStarted = false;
  bool _conversionReady = false;
  uint32_t _conversionStartMs = 0;
  int16_t _lastRawValue = 0;
};

} // namespace ADS1115
