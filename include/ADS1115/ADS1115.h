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

enum class ConfigurationState : uint8_t {
  UNBOUND,
  UNCONFIGURED,
  APPLYING,
  VERIFIED,
  UNKNOWN
};

enum class OperationKind : uint8_t {
  NONE,
  INITIALIZE,
  APPLY_PROFILE,
  RECOVER,
  READ_SINGLE_SHOT,
  SHUTDOWN
};

enum class OperationState : uint8_t {
  IDLE,
  ACTIVE,
  RECONCILING,
  SUCCEEDED,
  FAILED,
  CANCELLED,
  TIMED_OUT,
  INDETERMINATE
};

struct OperationToken {
  uint32_t value = 0;
  constexpr bool valid() const { return value != 0; }
};

enum class CancelDisposition : uint8_t {
  NO_ACTIVE_OPERATION,
  CANCELLED_BEFORE_IO,
  CANCELLED_AFTER_EFFECT,
  RECONCILIATION_REQUIRED
};

enum class SampleFlag : uint16_t {
  NONE = 0,
  CONFIG_VERIFIED = 1U << 0,
  AT_POSITIVE_CODE_LIMIT = 1U << 1,
  AT_NEGATIVE_CODE_LIMIT = 1U << 2
};

struct SampleResult {
  int16_t rawCode = 0;
  int32_t microvolts = 0;
  uint16_t channelId = 0;
  Mux mux = Mux::AIN0_GND;
  Gain gain = Gain::FSR_2_048V;
  DataRate dataRate = DataRate::SPS_128;
  uint16_t flags = 0;
  uint32_t configGeneration = 0;
  uint32_t sequence = 0;
};

struct AppliedProfileSnapshot {
  DeviceProfile profile{};
  ConfigurationState state = ConfigurationState::UNBOUND;
  uint32_t generation = 0;
};

struct OperationResult {
  OperationToken token{};
  OperationKind kind = OperationKind::NONE;
  OperationState state = OperationState::IDLE;
  Status status = Status::Ok();
  bool sampleValid = false;
  bool hardwareStateUncertain = false;
  SampleResult sample{};
};

/// @brief State of the optional poll-chunked job executor.
enum class JobState : uint8_t {
  IDLE,                         ///< No job has been scheduled.
  SINGLE_SHOT_WRITE_CONFIG,     ///< Next instruction writes config with OS/start bit.
  SINGLE_SHOT_WAIT_CONVERSION,  ///< Waiting for conversion time or ALERT/RDY.
  SINGLE_SHOT_POLL_READY,       ///< Next instruction reads CONFIG OS/ready bit.
  SINGLE_SHOT_READ_CONVERSION,  ///< Next instruction reads the conversion register.
  APPLY_WRITE_LOW_THRESHOLD,    ///< Next instruction writes the low threshold.
  APPLY_WRITE_HIGH_THRESHOLD,   ///< Next instruction writes the high threshold.
  APPLY_WRITE_CONFIG,           ///< Next instruction writes the CONFIG register.
  APPLY_VERIFY_LOW_THRESHOLD,   ///< Next instruction verifies the low threshold.
  APPLY_VERIFY_HIGH_THRESHOLD,  ///< Next instruction verifies the high threshold.
  APPLY_VERIFY_CONFIG,          ///< Next instruction verifies CONFIG writable fields.
  PROBE_CONFIG,                 ///< Next instruction probes CONFIG reachability.
  WAIT_IDLE_AFTER_ABANDON,      ///< Bus-silent wait before abandoned conversion reuse.
  SHUTDOWN_WRITE_CONFIG,        ///< Next instruction requests single-shot idle mode.
  COMPLETE,                     ///< Last job completed successfully.
  FAILED,                       ///< Last job failed; lastJobStatus() has details.
  CANCELLED,                    ///< Last job was safely cancelled.
  TIMED_OUT                     ///< Last job reached its deadline.
};

/// @brief Result returned by poll-chunked job calls.
struct PollResult {
  Status status = Status::Ok();       ///< Current or terminal job status.
  uint8_t instructionsUsed = 0;       ///< Transport callbacks used by this poll call.
  bool done = true;                   ///< True when the job is no longer active.
  JobState state = JobState::IDLE;    ///< State after this poll call.
  OperationToken token{};
  OperationKind kind = OperationKind::NONE;
  OperationState operationState = OperationState::IDLE;
};

/// @brief Snapshot of driver configuration and runtime state without I2C access.
struct SettingsSnapshot {
  bool initialized = false;
  bool bound = false;
  DriverState state = DriverState::UNINIT;
  ConfigurationState configurationState = ConfigurationState::UNBOUND;
  uint32_t configGeneration = 0;
  OperationKind operationKind = OperationKind::NONE;
  OperationState operationState = OperationState::IDLE;
  OperationToken operationToken{};
  bool terminalResultAvailable = false;
  uint8_t i2cAddress = 0x48;
  uint32_t i2cTimeoutMs = 0;
  uint8_t offlineThreshold = 0;
  bool strictInitVerify = false;
  bool hasNowMsHook = false;
  bool timebaseAvailable = false;
  bool hasGpioReadHook = false;
  bool hasCooperativeYieldHook = false;
  bool hardwareConfigDirty = false;
  Status hardwareConfigDirtyError = Status::Ok();
  uint8_t hardwareConfigDirtyAddress = 0x00;
  bool hardwareConfigUncertain = false;
  Status lastConfigApplyError = Status::Ok();
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
  /// Bind a non-owning transport and desired profile without I2C.
  Status bind(const DriverConfig& driverConfig, const DeviceProfile& profile);
  /// Schedule verified initialization without I2C.
  Status startInitialize(uint32_t nowMs, uint32_t deadlineMs, OperationToken& token);
  /// Schedule an atomic candidate-profile apply without I2C.
  Status startApplyProfile(const DeviceProfile& profile, uint32_t nowMs,
                           uint32_t deadlineMs, OperationToken& token);
  /// Schedule verified recovery without I2C.
  Status startRecover(uint32_t nowMs, uint32_t deadlineMs, OperationToken& token);
  /// Schedule one typed single-shot conversion without I2C.
  Status startRead(const ChannelRequest& request, uint32_t nowMs,
                   uint32_t deadlineMs, OperationToken& token);
  /// Schedule explicit shutdown without I2C.
  Status startShutdown(uint32_t nowMs, uint32_t deadlineMs, OperationToken& token);
  /// Advance the active operation by at most maxTransactions callbacks.
  PollResult poll(uint32_t nowMs, uint8_t maxTransactions = 1);
  /// Request cancellation without I2C; post-start work reconciles in poll().
  CancelDisposition cancelActiveOperation();
  /// Consume the pending terminal result exactly once by token.
  Status takeResult(OperationToken token, OperationResult& out);
  /// Drop the binding and all local state without I2C.
  void unbind();

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
  /// May perform one CONFIG-read I2C transaction when a single-shot conversion
  /// is pending and its conversion interval has elapsed. Failures are ignored
  /// by this compatibility API but remain visible through health counters,
  /// lastError(), and lastErrorMs() when a timebase is configured.
  /// @param nowMs Current monotonic time in milliseconds.
  void tick(uint32_t nowMs);
  /// Status-returning service step for pending conversion work.
  /// May perform one CONFIG-read I2C transaction when a single-shot conversion
  /// is pending and its conversion interval has elapsed.
  /// @param nowMs Current monotonic time in milliseconds.
  /// @return Immediate status from the service step, or Status::Ok() when no
  /// I2C work is needed.
  Status service(uint32_t nowMs);
  /// Bus-silent compatibility alias for unbind().
  void end();

  /// Request single-shot idle mode while keeping the driver initialized.
  /// Transaction count: one CONFIG write.
  /// @return Status::Ok() when the CONFIG write succeeds.
  Status shutdown();

  /// Check if begin() completed successfully and end() has not been called.
  /// @return true when the driver is initialized.
  bool isInitialized() const { return _initialized; }
  bool isBound() const { return _bound; }
  ConfigurationState configurationState() const { return _configurationState; }
  uint32_t configurationGeneration() const { return _configGeneration; }
  OperationToken activeOperationToken() const { return _operationToken; }
  OperationKind operationKind() const { return _operationKind; }
  OperationState operationState() const { return _operationState; }
  bool terminalResultAvailable() const { return _terminalResultAvailable; }
  Status getAppliedProfile(AppliedProfileSnapshot& out) const;

  /// Get the cached configuration snapshot currently owned by the driver.
  /// @return Cached driver configuration.
  const Config& getConfig() const { return _config; }

  // === Diagnostics (no health tracking) ===
  /// Probe ADS1115 CONFIG-register reachability without updating health counters.
  /// ADS1115 has no chip-ID register; this is a diagnostic I2C/register
  /// plausibility check, not identity proof.
  /// Requires a successfully initialized driver. begin() uses the same raw
  /// CONFIG-register probe internally before the driver is marked initialized.
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
  /// @return Compatibility alias for state().
  DriverState driverState() const { return state(); }
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
  /// @return Compatibility alias for hardwareConfigDirty().
  bool isHardwareConfigDirty() const { return hardwareConfigDirty(); }
  /// @return Compatibility alias for hardwareConfigDirty(); dirty means cache/hardware uncertainty.
  bool isHardwareConfigUncertain() const { return hardwareConfigDirty(); }
  /// @return Compatibility alias for hardwareConfigDirtyError().
  Status lastConfigApplyError() const { return hardwareConfigDirtyError(); }
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

  /// Compatibility wrapper around readConversionReady(). Returns false when the
  /// driver is not initialized or when the readiness path fails. False therefore
  /// means either "not ready" or "error"; production code should use
  /// readConversionReady(bool&) when the distinction matters.
  /// @return true when conversion data is ready to read.
  [[deprecated("Use readConversionReady(bool&)")]]
  bool conversionReady();

  /// Alias for readConversionReady() with explicit error reporting.
  /// @param[out] ready true when conversion data can be read
  /// @return Status from the readiness path
  Status conversionReady(bool& ready);

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
  /// Read the conversion register immediately as a signed two's-complement sample.
  /// This API does not check OS-bit readiness and does not wait for a fresh
  /// continuous-mode data-rate interval.
  /// @param[out] out Signed conversion code.
  /// @return Status::Ok() on a successful register read.
  Status readLatestRaw(int16_t& out);

  /// Start or join a single-shot conversion, or wait for a fresh continuous
  /// sample, with a finite deadline.
  /// Requires Config::nowMs; returns INVALID_CONFIG before starting conversion
  /// when no monotonic clock hook is configured.
  /// In continuous mode this waits until the configured data-rate interval marks
  /// a fresh sample ready, then reads the conversion register. Use
  /// readLatestRaw() when the caller intentionally wants the current register
  /// value immediately.
  /// Transaction count in single-shot mode: one CONFIG write to start plus
  /// conversion-register read; OS-bit polling can add CONFIG reads. Continuous
  /// mode performs one conversion-register read after readiness. Worst-case wall
  /// time is bounded by timeoutMs plus active I2C transaction timeouts. Polling
  /// occurs at most once per observed millisecond tick when I2C is needed; a
  /// stalled clock returns Err::CLOCK_STALLED after a finite same-tick guard.
  /// @param[out] out Signed conversion code.
  /// @param timeoutMs Maximum wait in milliseconds.
  /// @return Status::Ok() on success, Err::TIMEOUT when the deadline expires,
  /// or Err::CLOCK_STALLED when the supplied clock stops advancing.
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);

  /// Blocking read with voltage scaling.
  /// Requires Config::nowMs under the same contract as readBlocking().
  /// @param[out] volts Converted input voltage.
  /// @param timeoutMs Maximum wait in milliseconds.
  /// @return Status::Ok() on success, Err::TIMEOUT when the deadline expires,
  /// or Err::CLOCK_STALLED when the supplied clock stops advancing.
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);

  /// Start a poll-chunked single-shot conversion job without performing I2C.
  /// While any poll-chunked job is active, normal public I2C/configuration APIs
  /// return Err::BUSY; use the matching poll method or cancelJob().
  /// Use pollSingleShot() to advance the job with an explicit transaction budget.
  /// @return Err::IN_PROGRESS when the job is scheduled.
  Status startSingleShot();

  /// Start a poll-chunked single-shot conversion job for a mux without I2C.
  /// @param mux Input mux to use for this conversion.
  /// @return Err::IN_PROGRESS when the job is scheduled.
  Status startSingleShot(Mux mux);

  /// Advance a single-shot job by at most maxInstructions transport callbacks.
  /// maxInstructions is clamped to 3; passing 0 performs no transport work.
  /// Delay gates return with instructionsUsed == 0.
  /// @param nowMs Current monotonic time in milliseconds.
  /// @param maxInstructions Maximum transport callbacks to perform this poll.
  /// @return Job progress, terminal status, and callbacks consumed.
  PollResult pollSingleShot(uint32_t nowMs, uint8_t maxInstructions = 1);

  /// Start a staged cached-config apply job without performing I2C.
  /// While any poll-chunked job is active, normal public I2C/configuration APIs
  /// return Err::BUSY; use the matching poll method or cancelJob().
  /// Normal continuous-mode background conversion state is allowed; an active
  /// single-shot conversion is rejected with Err::BUSY.
  /// The job writes low threshold, high threshold, CONFIG, and performs strict
  /// or dirty-state readback when required by the current cache state.
  /// @return Err::IN_PROGRESS when the job is scheduled.
  Status startApplyConfigJob();

  /// Advance a config-apply job by at most maxInstructions transport callbacks.
  /// maxInstructions is clamped to 3; passing 0 performs no transport work.
  /// @param nowMs Current monotonic time in milliseconds; reserved for symmetry.
  /// @param maxInstructions Maximum transport callbacks to perform this poll.
  /// @return Job progress, terminal status, and callbacks consumed.
  PollResult pollApplyConfig(uint32_t nowMs, uint8_t maxInstructions = 1);

  /// Cancel the active poll-chunked job without touching hardware.
  void cancelJob();
  /// @return true when a poll-chunked job is active.
  bool jobActive() const { return _jobActive; }
  /// @return Current poll-chunked job state.
  JobState jobState() const { return _jobState; }
  /// @return Terminal or current status of the last poll-chunked job.
  Status lastJobStatus() const { return _lastJobStatus; }
  /// @return Last raw conversion value captured by read APIs or poll jobs.
  int16_t lastRawValue() const { return _lastRawValue; }

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
  /// be short; continuous-mode pulses are approximately 8 us per the datasheet
  /// caveat. Use an interrupt-capable input or latching strategy when polling
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

  // === Internal ===
  Status _readConversionReadyAt(uint32_t nowMs, bool& ready);
  Status _probeRaw();
  Status _applyConfig();
  Status _writeConfigOnly();
  Status _verifyConfigReadback();
  void _markHardwareConfigDirty(const Status& st);
  void _markHardwareConfigDirtyIfClean(const Status& st);
  void _clearHardwareConfigDirty();
  uint16_t _buildConfigRegister() const;
  uint16_t _buildConfigRegisterForMux(Mux mux) const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;
  Status _jobBusyStatus() const;
  uint8_t _instructionBudget(uint8_t maxInstructions) const;
  PollResult _pollResult(Status status, uint8_t instructionsUsed, bool done) const;
  PollResult _finishOperation(const Status& status, OperationState state,
                              uint8_t transactionsUsed, bool sampleValid = false);
  Status _beginOperation(OperationKind kind, uint32_t nowMs, uint32_t deadlineMs,
                         OperationToken& token);
  bool _deadlineReached(uint32_t nowMs) const;
  bool _singleShotMayBeActive() const;
  Status _activeHardwareBusyStatus() const;
  void _resetOperationScratch();
  void _loadProfileIntoConfig(const DeviceProfile& profile);
  DeviceProfile _profileFromConfig() const;
  uint16_t _buildConfigRegisterFor(const DeviceProfile& profile, Mux mux,
                                   Gain gain) const;
  Status _verifyJobReadback(uint8_t reg, uint16_t expected, const char* message);

  // === State ===
  static constexpr uint8_t MAX_JOB_INSTRUCTIONS = 3;
  static constexpr uint16_t kMaxSameTickPolls = 1024U;
  static constexpr uint8_t kInvalidDirtyAddress = 0x00;

  Config _config;
  DriverConfig _driverConfig;
  DeviceProfile _desiredProfile;
  DeviceProfile _appliedProfile;
  DeviceProfile _candidateProfile;
  bool _bound = false;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;
  bool _hardwareConfigDirty = false;
  Status _hardwareConfigDirtyError = Status::Ok();
  uint8_t _hardwareConfigDirtyAddress = kInvalidDirtyAddress;
  ConfigurationState _configurationState = ConfigurationState::UNBOUND;
  ConfigurationState _configurationStateBeforeOperation = ConfigurationState::UNBOUND;
  uint32_t _configGeneration = 0;

  // === Poll-Chunked Job State ===
  bool _jobActive = false;
  JobState _jobState = JobState::IDLE;
  Status _lastJobStatus = Status::Ok();
  uint16_t _jobConfigRegister = 0;
  int16_t _jobThresholdLow = 0;
  int16_t _jobThresholdHigh = 0;
  Mux _jobMux = Mux::AIN0_GND;
  Gain _jobGain = Gain::FSR_2_048V;
  ChannelRequest _channelRequest{};
  bool _jobStartWriteAttempted = false;
  bool _jobAnyWriteConfirmed = false;
  uint32_t _jobNextReadyPollMs = 0;
  Status _abandonStatus = Status::Ok();

  // === Owner Operation State ===
  OperationKind _operationKind = OperationKind::NONE;
  OperationState _operationState = OperationState::IDLE;
  OperationToken _operationToken{};
  uint32_t _nextOperationToken = 1;
  uint32_t _operationStartMs = 0;
  uint32_t _operationDeadlineMs = 0;
  uint32_t _activeTransferTimeoutMs = 0;
  bool _terminalResultAvailable = false;
  OperationResult _terminalResult{};
  SampleResult _workingSample{};
  uint32_t _sampleSequence = 0;

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
  uint8_t _continuousSettlePeriods = 1;
  int16_t _lastRawValue = 0;
};

} // namespace ADS1115
