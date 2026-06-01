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

/// @brief Coarse driver health state.
enum class DriverState : uint8_t {
  UNINIT,    ///< begin() not called or end() called
  READY,     ///< Operational, consecutiveFailures == 0
  DEGRADED,  ///< 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    ///< consecutiveFailures >= offlineThreshold
};

/// @brief Snapshot of driver configuration and runtime state without I2C access.
struct SettingsSnapshot {
  bool initialized = false;
  DriverState state = DriverState::UNINIT;
  uint8_t i2cAddress = 0x48;
  uint32_t i2cTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  bool strictInitVerify = false;
  bool hasNowMsHook = false;
  bool hasGpioReadHook = false;
  bool hasCooperativeYieldHook = false;
  bool hardwareConfigDirty = false;
  Status hardwareConfigDirtyError = Status::Ok();
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

/// @brief Transport-agnostic ADS1115 driver.
///
/// The driver is externally serialized: it does not provide internal locking
/// and is not safe for concurrent calls from multiple tasks. Public APIs can
/// perform blocking I2C through the injected transport and are not ISR-safe.
///
/// Latency model: each I2C transaction can block up to Config::i2cTimeoutMs.
/// Register read/write APIs perform one transaction. CONFIG-only setters
/// perform one write. Full resync paths such as begin(), recover(), and
/// enableConversionReadyPin() can perform three writes, plus optional strict
/// read-back verification reads.
class ADS1115 {
public:
  ADS1115() = default;
  ADS1115(const ADS1115&) = delete;
  ADS1115& operator=(const ADS1115&) = delete;
  ADS1115(ADS1115&&) = delete;
  ADS1115& operator=(ADS1115&&) = delete;
  ~ADS1115() = default;

  // === Lifecycle ===
  /// Initialize the driver with configuration and verify device presence.
  /// ADS1115 has no ID register; strictInitVerify adds a register read-back
  /// plausibility check with dynamic CONFIG OS/status bits masked out.
  /// Transaction count: one CONFIG read plus three writes; strict mode adds
  /// three read-back transactions.
  /// If begin() fails after one or more writes may have reached hardware,
  /// hardwareConfigDirty() and hardwareConfigDirtyError() remain available even
  /// though the driver is not initialized. A later successful full apply clears
  /// the dirty diagnostic.
  /// @param config Transport callbacks, device address, timing, and conversion settings.
  /// @return Status::Ok() when the device responds and cached configuration is applied.
  Status begin(const Config& config);
  /// Process pending operations (currently bounded polling only).
  /// @param nowMs Current monotonic time in milliseconds.
  void tick(uint32_t nowMs);
  /// Best-effort shutdown to single-shot idle and clear cached conversion state.
  /// The shutdown write result is intentionally ignored; use shutdown() when the
  /// application needs an explicit I2C status.
  void end();

  /// Request single-shot idle mode while keeping the driver initialized.
  /// Transaction count: one CONFIG write.
  /// @return Status::Ok() when the CONFIG write succeeds.
  Status shutdown();

  /// Check if begin() completed successfully and end() has not been called.
  /// @return true when the driver is initialized.
  bool isInitialized() const { return _initialized; }

  /// Get the cached configuration snapshot currently owned by the driver.
  /// @return Cached driver configuration.
  const Config& getConfig() const { return _config; }

  // === Diagnostics (no health tracking) ===
  /// Probe the device without updating health counters.
  /// Address NACK maps to DEVICE_NOT_FOUND; distinguishable timeout, bus, data
  /// NACK, and generic I2C failures are preserved.
  /// Transaction count: one CONFIG read.
  /// @return Status::Ok() when the CONFIG register can be read.
  Status probe();
  /// Attempt recovery from DEGRADED or OFFLINE state using tracked I2C.
  /// Transaction count: one CONFIG read plus three writes; strict mode adds
  /// three read-back transactions.
  /// @return Status::Ok() when the device responds and cached configuration is restored.
  Status recover();

  /// Populate a snapshot of cached configuration and runtime state without I2C.
  /// @param[out] out Snapshot to fill
  /// @return Status::Ok() always
  Status getSettings(SettingsSnapshot& out) const;

  // === Driver State ===
  /// @return Current coarse driver state.
  DriverState state() const { return _driverState; }
  /// @return true when the driver is READY or DEGRADED.
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

  // === Health Tracking ===
  /// @return Timestamp of the last successful tracked I2C operation.
  uint32_t lastOkMs() const { return _lastOkMs; }
  /// @return Timestamp of the last failed tracked I2C operation.
  uint32_t lastErrorMs() const { return _lastErrorMs; }
  /// @return Most recent tracked error.
  Status lastError() const { return _lastError; }
  /// @return true when a failed multi-register update may have left hardware out of sync.
  bool hardwareConfigDirty() const { return _hardwareConfigDirty; }
  /// @return Original Status from the failed transaction that dirtied hardware config.
  Status hardwareConfigDirtyError() const { return _hardwareConfigDirtyError; }
  /// @return Consecutive tracked failures since the last success.
  uint8_t consecutiveFailures() const { return _consecutiveFailures; }
  /// @return Lifetime tracked failure count.
  uint32_t totalFailures() const { return _totalFailures; }
  /// @return Lifetime tracked success count.
  uint32_t totalSuccess() const { return _totalSuccess; }

  // === Conversion API ===
  /// Start one single-shot conversion using the cached mux.
  /// @return Err::IN_PROGRESS when started, Err::UNSUPPORTED_OPERATION in
  /// continuous mode, or Err::BUSY when a single-shot conversion is already active.
  Status startConversion();

  /// Start one single-shot conversion after applying the requested mux.
  /// The cached mux is restored if the CONFIG write fails.
  /// @param mux Input mux to use for this conversion.
  /// @return Err::IN_PROGRESS when started, Err::UNSUPPORTED_OPERATION in
  /// continuous mode, or Err::BUSY when a single-shot conversion is already active.
  Status startConversion(Mux mux);

  /// Convenience wrapper around readConversionReady(). Returns false when the
  /// driver is not initialized or when the underlying CONFIG read fails.
  /// @return true when conversion data is ready to read.
  bool conversionReady();

  /// Check conversion readiness with explicit error reporting.
  /// Uses ALERT/RDY when configured for conversion-ready mode. Otherwise,
  /// single-shot mode polls the OS bit after the conversion time and continuous
  /// mode tracks the configured data-rate interval between fresh samples.
  /// Transaction count: zero when cached/timing/ALERT path is enough; otherwise
  /// one CONFIG read in single-shot OS-bit polling.
  /// @param[out] ready true when conversion data can be read
  /// @return Status from the readiness path
  Status readConversionReady(bool& ready);

  /// Read the conversion register as a signed two's-complement sample.
  /// In continuous mode this returns the latest register value immediately and
  /// does not wait for a fresh data-rate interval. Use readConversionReady()
  /// first when the caller requires a fresh continuous sample indication.
  /// Transaction count: one conversion-register read after readiness checks.
  /// @param[out] out Signed conversion code.
  /// @return Status::Ok() on a successful register read.
  Status readRaw(int16_t& out);

  /// Read a signed sample and scale it using the active gain.
  /// @param[out] volts Converted input voltage.
  /// @return Status::Ok() on a successful sample read and conversion.
  Status readVoltage(float& volts);

  /// Read the latest conversion register value immediately.
  /// In continuous mode this is the latest register contents and does not wait
  /// for a fresh sample interval. In single-shot mode this bypasses readiness
  /// checks and should be used only when the caller has established readiness.
  /// @param[out] out Signed conversion code.
  /// @return Status::Ok() on a successful register read.
  Status readLatestRaw(int16_t& out);

  /// Start or join a single-shot conversion and wait with a finite deadline.
  /// Requires Config::nowMs; returns INVALID_CONFIG before starting conversion
  /// when no monotonic clock hook is configured.
  /// Transaction count: one CONFIG write to start plus conversion-register read;
  /// OS-bit polling can add CONFIG reads.
  /// @param[out] out Signed conversion code.
  /// @param timeoutMs Maximum wait in milliseconds.
  /// @return Status::Ok() on success, Err::TIMEOUT when the deadline expires.
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);

  /// Blocking read with voltage scaling.
  /// Requires Config::nowMs under the same contract as readBlocking().
  /// @param[out] volts Converted input voltage.
  /// @param timeoutMs Maximum wait in milliseconds.
  /// @return Status::Ok() on success, Err::TIMEOUT when the deadline expires.
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);

  // === Configuration ===
  /// Set the input multiplexer. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write.
  /// @param mux Input mux selection.
  /// @return Status::Ok() when CONFIG was written.
  Status setMux(Mux mux);
  /// @return Cached input mux selection.
  Mux getMux() const { return _config.mux; }

  /// Set PGA full-scale range. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write. PGA full-scale range does not relax
  /// ADS1115 analog input absolute limits; keep inputs within datasheet limits.
  /// @param gain Full-scale range selection.
  /// @return Status::Ok() when CONFIG was written.
  Status setGain(Gain gain);
  /// @return Cached PGA full-scale range.
  Gain getGain() const { return _config.gain; }

  /// Set output data rate. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write.
  /// @param rate Output sample rate.
  /// @return Status::Ok() when CONFIG was written.
  Status setDataRate(DataRate rate);
  /// @return Cached output sample rate.
  DataRate getDataRate() const { return _config.dataRate; }

  /// Set operating mode. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write.
  /// @param mode Single-shot or continuous conversion mode.
  /// @return Status::Ok() when CONFIG was written.
  Status setMode(Mode mode);
  /// @return Cached conversion mode.
  Mode getMode() const { return _config.mode; }

  /// Read the ADS1115 CONFIG register.
  /// Transaction count: one CONFIG read.
  /// @param[out] config Raw 16-bit CONFIG register.
  /// @return Status::Ok() on a successful register read.
  Status readConfig(uint16_t& config);

  /// Write a validated CONFIG register value and sync the typed cache.
  /// Transaction count: one CONFIG write.
  /// @param config Raw 16-bit CONFIG value. PGA aliases 110b and 111b map to Gain::FSR_0_256V.
  /// @return Status::Ok() when the register is written and cache is updated.
  Status writeConfig(uint16_t config);

  // === Raw Register Access ===
  /// Read a 16-bit register using tracked I2C access.
  /// Valid register pointers are 0x00..0x03.
  /// @param reg Register pointer.
  /// @param[out] value Raw 16-bit register value.
  /// @return Status::Ok() on a successful register read.
  Status readRegister16(uint8_t reg, uint16_t& value);

  /// Write a 16-bit register using tracked I2C access.
  /// Writable register pointers are 0x01..0x03; conversion register 0x00 is read-only.
  /// @param reg Register pointer.
  /// @param value Raw 16-bit value to write.
  /// Successful raw writes are diagnostic writes: they leave the typed cache
  /// unchanged and mark hardwareConfigDirty() with Err::HARDWARE_CONFIG_DIRTY.
  /// hardwareConfigDirtyError().detail stores the register pointer.
  /// If the transport reports an error after the raw write is attempted, the
  /// same transport Status is preserved as the dirty diagnostic because hardware
  /// may have accepted the write.
  /// Dirty state clears only after a later full cached-settings rewrite and
  /// successful read-back verification.
  /// @return Status::Ok() on success; Err::INVALID_PARAM for read-only register
  /// 0x00 or invalid registers above 0x03.
  Status writeRegister16(uint8_t reg, uint16_t value);

  /// Compatibility alias for readRegister16().
  /// @param reg Register pointer.
  /// @param[out] value Raw 16-bit register value.
  /// @return Status::Ok() on a successful register read.
  Status readRegister(uint8_t reg, uint16_t& value) { return readRegister16(reg, value); }

  /// Compatibility alias for writeRegister16(); inherits the same diagnostic
  /// dirty/stale cache behavior.
  /// @param reg Register pointer.
  /// @param value Raw 16-bit value to write.
  /// @return Status::Ok() on a successful register write.
  Status writeRegister(uint8_t reg, uint16_t value) { return writeRegister16(reg, value); }

  // === Comparator ===
  /// Set signed comparator thresholds. Cache changes commit after both writes succeed.
  /// Thresholds are signed raw conversion codes and must be recalculated if the
  /// gain/full-scale range changes. If the second write fails after the first
  /// reached hardware, hardwareConfigDirty() is set with the original error.
  /// Transaction count: two threshold writes.
  /// @param low Low threshold raw code.
  /// @param high High threshold raw code.
  /// @return Status::Ok() when both threshold registers are written.
  Status setThresholds(int16_t low, int16_t high);

  /// Read signed comparator thresholds and sync the cache.
  /// Transaction count: two threshold reads.
  /// @param[out] low Low threshold raw code.
  /// @param[out] high High threshold raw code.
  /// @return Status::Ok() when both threshold registers are read.
  Status getThresholds(int16_t& low, int16_t& high);

  /// Set comparator mode. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write.
  /// @param mode Traditional or window comparator mode.
  /// @return Status::Ok() when CONFIG was written.
  Status setComparatorMode(ComparatorMode mode);
  /// @return Cached comparator mode.
  ComparatorMode getComparatorMode() const { return _config.compMode; }

  /// Set ALERT/RDY polarity. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write.
  /// @param polarity ALERT/RDY active polarity.
  /// @return Status::Ok() when CONFIG was written.
  Status setComparatorPolarity(ComparatorPolarity polarity);
  /// @return Cached comparator polarity.
  ComparatorPolarity getComparatorPolarity() const { return _config.compPolarity; }

  /// Set comparator latch behavior. Cache changes commit only after I2C success.
  /// Transaction count: one CONFIG write.
  /// @param latch Latching or non-latching behavior.
  /// @return Status::Ok() when CONFIG was written.
  Status setComparatorLatch(ComparatorLatch latch);
  /// @return Cached comparator latch behavior.
  ComparatorLatch getComparatorLatch() const { return _config.compLatch; }

  /// Set comparator queue depth or disable comparator.
  /// Transaction count: one CONFIG write.
  /// @param queue Comparator assertion queue depth.
  /// @return Status::Ok() when CONFIG was written.
  Status setComparatorQueue(ComparatorQueue queue);
  /// @return Cached comparator queue setting.
  ComparatorQueue getComparatorQueue() const { return _config.compQueue; }

  /// @return true when Config::alertRdyPin is configured.
  bool isAlertRdyPinConfigured() const;
  /// @return true when comparator thresholds are configured for conversion-ready mode.
  bool isConversionReadyModeEnabled() const;
  /// @return true when conversion readiness uses the ALERT/RDY GPIO callback.
  bool usesAlertRdyPinForConversionReady() const;

  /// Program ADS1115 threshold/comparator fields for conversion-ready ALERT/RDY mode.
  /// ALERT/RDY is open-drain and requires a pull-up. Conversion-ready pulses can
  /// be short; use an interrupt-capable input or latching strategy when polling
  /// cannot guarantee capture.
  /// Transaction count: three writes; strict mode adds three read-back reads.
  /// @return Status::Ok() when thresholds and CONFIG are written.
  Status enableConversionReadyPin();

  /// Disable comparator output by setting queue to DISABLE.
  /// Transaction count: one CONFIG write.
  /// @return Status::Ok() when CONFIG was written.
  Status disableComparator();

  // === Utility ===
  /// @param raw Signed conversion code.
  /// @return Input voltage using the cached PGA range.
  float rawToVoltage(int16_t raw) const;
  /// @return LSB size in volts for the cached PGA range.
  float getLsbVoltage() const;
  /// @return Conservative conversion time in milliseconds for the cached data rate.
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
  void _reassertOfflineLatch();

  // === Internal ===
  Status _readConversionReadyAt(uint32_t nowMs, bool& ready);
  Status _applyConfig();
  Status _writeConfigOnly();
  Status _verifyConfigReadback();
  void _markHardwareConfigDirty(const Status& st);
  void _clearHardwareConfigDirty();
  uint16_t _buildConfigRegister() const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;

  // === State ===
  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;
  bool _allowOfflineI2c = false;
  bool _hardwareConfigDirty = false;
  Status _hardwareConfigDirtyError = Status::Ok();

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
