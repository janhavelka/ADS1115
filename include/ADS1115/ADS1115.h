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
  UNINIT,    ///< No active binding or initialization has not completed
  READY,     ///< Operational, consecutiveFailures == 0
  DEGRADED,  ///< 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    ///< Diagnostic threshold reached; does not suppress owner I/O
};

/// @brief Trust state of the cached/applied device profile.
enum class ConfigurationState : uint8_t {
  UNBOUND,      ///< No transport/profile binding exists
  UNCONFIGURED, ///< Bound, but no profile has been verified on hardware
  APPLYING,     ///< A profile-changing owner operation is active
  VERIFIED,     ///< Full threshold and masked CONFIG readback succeeded
  UNKNOWN       ///< Hardware/cache agreement is not safe to assume
};

/// @brief Operation classes owned by the poll-driven API.
enum class OperationKind : uint8_t {
  NONE,              ///< No owner operation
  INITIALIZE,        ///< Probe, apply, and verify initial profile
  APPLY_PROFILE,     ///< Apply and verify a same-address profile
  RECOVER,           ///< Probe, replay, and verify the desired profile
  READ_SINGLE_SHOT,  ///< Start, verify, and read one typed conversion
  SHUTDOWN           ///< Request and verify single-shot idle mode
};

/// @brief Lifecycle of one tokened owner operation.
enum class OperationState : uint8_t {
  IDLE,          ///< No active operation or pending result
  ACTIVE,        ///< Operation may perform bounded work through poll()
  RECONCILING,   ///< Bus-silent wait for a possibly active conversion
  SUCCEEDED,     ///< Terminal successful result
  FAILED,        ///< Terminal definite failure
  CANCELLED,     ///< Terminal caller cancellation
  TIMED_OUT,     ///< Terminal whole-operation deadline expiry
  INDETERMINATE  ///< Terminal state with uncertain hardware effect
};

/// @brief Nonzero identity assigned when an owner operation is accepted.
///
/// Tokens remain reserved through terminal completion and must be passed to
/// takeResult(). A new operation is rejected until that result is consumed.
struct OperationToken {
  uint32_t value = 0; ///< Zero means no accepted operation

  /// @brief Check whether the token identifies an accepted operation.
  /// @return true when this token identifies an accepted operation.
  constexpr bool valid() const { return value != 0; }
};

/// @brief Immediate, bus-silent cancellation disposition.
enum class CancelDisposition : uint8_t {
  NO_ACTIVE_OPERATION,       ///< Nothing was active to cancel
  CANCELLED_BEFORE_EFFECT,   ///< Terminal cancellation before hardware effect
  CANCELLED_AFTER_EFFECT,    ///< Terminal cancellation after a safe known effect
  RECONCILIATION_REQUIRED    ///< Bus-silent quiet interval must finish first
};

/// @brief Provenance and boundary flags carried by SampleResult::flags.
enum class SampleFlag : uint16_t {
  NONE = 0,                              ///< No flags set
  CONFIG_VERIFIED = 1U << 0,             ///< Sample uses verified clean config
  AT_POSITIVE_CODE_LIMIT = 1U << 1,      ///< Raw code equals INT16_MAX
  AT_NEGATIVE_CODE_LIMIT = 1U << 2       ///< Raw code equals INT16_MIN
};

/// @brief Atomic fixed-memory result for one verified single-shot conversion.
///
/// microvolts is the ADC-input value for the recorded PGA gain. Board divider,
/// shunt, amplifier, offset, calibration, and engineering-unit conversion stay
/// in the application.
struct SampleResult {
  int16_t rawCode = 0; ///< Signed two's-complement conversion code
  int32_t microvolts = 0; ///< Deterministically rounded nominal ADC-input value
  uint16_t channelId = 0; ///< Application identity copied from ChannelRequest
  Mux mux = Mux::AIN0_GND; ///< MUX used for this sample
  Gain gain = Gain::FSR_2_048V; ///< PGA range used for scaling
  DataRate dataRate = DataRate::SPS_128; ///< Data rate used for conversion timing
  uint16_t flags = 0; ///< Bitwise SampleFlag values
  uint32_t configGeneration = 0; ///< Verified profile generation used
  uint32_t sequence = 0; ///< Successful-sample sequence; wraps UINT32_MAX to 1
};

/// @brief Bus-silent snapshot of the last committed profile record and trust state.
///
/// Treat profile as verified hardware configuration only when state is VERIFIED.
/// UNCONFIGURED means no profile has committed; UNKNOWN means the retained last
/// committed record may no longer match hardware.
/// A typed read updates profile.defaultMux/defaultGain to the channel it verified,
/// so those two fields track what is latched in the device CONFIG register.
/// startRecover() and startApplyProfile() replay the owner's desired profile.
struct AppliedProfileSnapshot {
  DeviceProfile profile{}; ///< Last committed record; validity is qualified by state
  ConfigurationState state = ConfigurationState::UNBOUND; ///< Current trust state
  uint32_t generation = 0; ///< Increments after each successful verified commit
};

/// @brief Exactly-once terminal result of a tokened owner operation.
struct OperationResult {
  OperationToken token{}; ///< Token assigned when the operation was accepted
  OperationKind kind = OperationKind::NONE; ///< Completed operation kind
  OperationState state = OperationState::IDLE; ///< Terminal lifecycle state
  Status status = Status::Ok(); ///< Final operation outcome
  bool sampleValid = false; ///< True only when sample contains a publishable read
  bool hardwareStateUncertain = false; ///< Hardware effects may be partial/unknown
  SampleResult sample{}; ///< Atomic sample; meaningful only when sampleValid is true
};

/// @brief State of the optional poll-chunked job executor.
enum class JobState : uint8_t {
  IDLE,                         ///< No job has been scheduled.
  SINGLE_SHOT_WRITE_CONFIG,     ///< Next instruction writes config with OS/start bit.
  SINGLE_SHOT_WAIT_CONVERSION,  ///< Bus-silent wait before the next OS readiness read.
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
  OperationToken token{};             ///< Active or terminal operation token.
  OperationKind kind = OperationKind::NONE; ///< Active or terminal operation kind.
  OperationState operationState = OperationState::IDLE; ///< Owner lifecycle state.
};

/// @brief Snapshot of driver configuration and runtime state without I2C access.
struct SettingsSnapshot {
  bool initialized = false; ///< Compatibility begin() completed successfully
  bool bound = false; ///< A valid transport/profile binding exists
  DriverState state = DriverState::UNINIT; ///< Passive transport-health state
  ConfigurationState configurationState = ConfigurationState::UNBOUND; ///< Trust state
  uint32_t configGeneration = 0; ///< Verified profile generation
  OperationKind operationKind = OperationKind::NONE; ///< Current owner operation kind
  OperationState operationState = OperationState::IDLE; ///< Current owner state
  OperationToken operationToken{}; ///< Active or unconsumed terminal token
  bool terminalResultAvailable = false; ///< takeResult() can consume a result
  uint8_t i2cAddress = 0x48; ///< Currently bound seven-bit address
  uint32_t i2cTimeoutMs = 0; ///< Configured per-transfer timeout cap
  uint8_t offlineThreshold = 0; ///< Passive consecutive-failure threshold
  bool strictInitVerify = false; ///< Compatibility flag; production always verifies
  bool hasNowMsHook = false; ///< Compatibility monotonic hook is configured
  bool timebaseAvailable = false; ///< Compatibility monotonic hook is available
  bool hasGpioReadHook = false; ///< Compatibility GPIO callback is configured
  bool hasCooperativeYieldHook = false; ///< Blocking compatibility yield is configured
  bool hardwareConfigDirty = false; ///< Hardware/cache agreement is not trusted
  Status hardwareConfigDirtyError = Status::Ok(); ///< Error that dirtied trust
  uint8_t hardwareConfigDirtyAddress = 0x00; ///< Address affected by dirty write
  bool hardwareConfigUncertain = false; ///< Compatibility alias of dirty state
  Status lastConfigApplyError = Status::Ok(); ///< Compatibility dirty-error alias
  int alertRdyPin = -1; ///< Configured ALERT/RDY GPIO number
  bool alertRdyPinConfigured = false; ///< ALERT/RDY pin and GPIO callback are configured
  bool conversionReadyModeEnabled = false; ///< Thresholds encode ready mode
  bool usesAlertRdyPin = false; ///< GPIO readiness path is active
  Mux mux = Mux::AIN0_GND; ///< Cached MUX selection
  Gain gain = Gain::FSR_2_048V; ///< Cached PGA range
  DataRate dataRate = DataRate::SPS_128; ///< Cached data rate
  Mode mode = Mode::SINGLE_SHOT; ///< Cached conversion mode
  ComparatorMode compMode = ComparatorMode::TRADITIONAL; ///< Comparator mode
  ComparatorPolarity compPolarity = ComparatorPolarity::ACTIVE_LOW; ///< Polarity
  ComparatorLatch compLatch = ComparatorLatch::NON_LATCHING; ///< Latch policy
  ComparatorQueue compQueue = ComparatorQueue::DISABLE; ///< Queue setting
  int16_t compThresholdHigh = 0x7FFF; ///< Cached signed high threshold
  int16_t compThresholdLow = static_cast<int16_t>(0x8000); ///< Cached low threshold
  bool conversionStarted = false; ///< A direct conversion is outstanding
  bool conversionReady = false; ///< Cached direct-conversion readiness
  uint32_t conversionStartMs = 0; ///< Direct conversion start timestamp
  int16_t lastRawValue = 0; ///< Most recent successfully read raw code
};

/// @brief Transport-agnostic ADS1115 driver.
///
/// The driver is externally serialized: it does not provide internal locking
/// and is not safe for concurrent calls from multiple tasks. Public APIs can
/// perform blocking I2C through the injected transport and are not ISR-safe.
///
/// The owner-safe API is bind()/start*()/poll()/takeResult()/unbind(). Start,
/// cancel, result consumption, and unbind are bus-silent. poll() is the only
/// owner-safe method that calls transport callbacks and honors the caller's
/// transaction budget (clamped to three). Callback timeout caps are partitioned
/// so their sum cannot exceed the remaining whole-operation time sampled at the
/// poll boundary; each is also capped by DriverConfig::transferTimeoutMs.
///
/// All public methods require external serialization and task context. The
/// driver owns no bus, lock, GPIO, clock, retry policy, recovery policy, or
/// scheduler and performs no heap allocation in steady operation.
///
/// The Config/begin/direct-setter surface is retained for compatibility and
/// diagnostics. Those methods can perform multiple synchronous transfers and
/// are not the production shared-bus ownership model.
class ADS1115 {
public:
  ADS1115() = default;
  ADS1115(const ADS1115&) = delete;
  ADS1115& operator=(const ADS1115&) = delete;
  ADS1115(ADS1115&&) = delete;
  ADS1115& operator=(ADS1115&&) = delete;
  ~ADS1115() = default;

  /// @name Lifecycle and owner operations
  /// @{

  /// Bind a validated non-owning transport and desired profile without I2C.
  /// @param driverConfig Required callback binding and transfer timeout cap.
  /// @param profile Complete desired volatile hardware profile.
  /// @return OK, INVALID_CONFIG without changing the prior binding, or BUSY
  ///         while an operation/result/direct conversion prevents rebinding.
  Status bind(const DriverConfig& driverConfig, const DeviceProfile& profile);
  /// Schedule probe, full profile apply, and mandatory readback without I2C.
  /// @param nowMs Current owner monotonic time.
  /// @param deadlineMs Absolute wrap-safe deadline in the same time domain;
  ///        it must be in the future by no more than INT32_MAX milliseconds.
  /// @param[out] token Nonzero operation identity on acceptance.
  /// Requires a valid binding but does not require prior initialization.
  /// @return IN_PROGRESS when scheduled; NOT_INITIALIZED when unbound; BUSY for
  /// active/pending work; or INVALID_PARAM for an invalid deadline.
  Status startInitialize(uint32_t nowMs, uint32_t deadlineMs, OperationToken& token);
  /// Schedule a validated candidate-profile apply and readback without I2C.
  /// The candidate commits only after all writable fields are verified.
  /// The I2C address cannot change; unbind and bind a new profile instead.
  /// Requires successful initialization.
  /// @param profile Candidate profile using the currently bound address.
  /// @param nowMs Current owner monotonic time.
  /// @param deadlineMs Absolute wrap-safe deadline in the same time domain.
  /// @param[out] token Nonzero operation identity on acceptance.
  /// @return IN_PROGRESS when scheduled, or a validation/precondition status.
  Status startApplyProfile(const DeviceProfile& profile, uint32_t nowMs,
                           uint32_t deadlineMs, OperationToken& token);
  /// Schedule owner-authorized probe and verified profile replay without I2C.
  /// Requires a valid binding but can recover a failed initialization.
  /// @param nowMs Current owner monotonic time.
  /// @param deadlineMs Absolute wrap-safe deadline in the same time domain.
  /// @param[out] token Nonzero operation identity on acceptance.
  /// @return IN_PROGRESS when scheduled, or a precondition status.
  Status startRecover(uint32_t nowMs, uint32_t deadlineMs, OperationToken& token);
  /// Schedule one typed, provenance-preserving single-shot conversion without I2C.
  /// Requires successful initialization and a VERIFIED single-shot profile with
  /// clean hardware state.
  /// @param request Application channel identity, MUX, and PGA for the sample.
  /// @param nowMs Current owner monotonic time.
  /// @param deadlineMs Absolute wrap-safe deadline in the same time domain.
  /// @param[out] token Nonzero operation identity on acceptance.
  /// @return IN_PROGRESS when scheduled, or a validation/precondition status.
  Status startRead(const ChannelRequest& request, uint32_t nowMs,
                   uint32_t deadlineMs, OperationToken& token);
  /// Schedule explicit single-shot-idle shutdown and CONFIG/OS readback without I2C.
  /// Requires successful initialization. Accepted work immediately moves
  /// configurationState() to APPLYING until the readback completes or fails.
  /// A continuous-to-single-shot transition waits one worst-case conversion
  /// interval from the first post-write poll before checking OS, so success
  /// proves the device is no longer converting. The deadline must cover that
  /// interval, all planned callbacks, readiness retries, and owner scheduling.
  /// Unknown or dirty configuration uses the conservative 8-SPS idle bound.
  /// @param nowMs Current owner monotonic time.
  /// @param deadlineMs Absolute wrap-safe deadline in the same time domain.
  /// @param[out] token Nonzero operation identity on acceptance.
  /// @return IN_PROGRESS when scheduled, or a precondition status.
  Status startShutdown(uint32_t nowMs, uint32_t deadlineMs, OperationToken& token);
  /// Advance the active operation by at most maxTransactions callbacks.
  /// @param nowMs Current time in the operation's original time domain.
  /// @param maxTransactions Callback budget, clamped to three and to the whole
  ///        milliseconds remaining before the deadline; zero is bus-silent.
  /// @return Current/terminal status, budget used, token, and operation state.
  PollResult poll(uint32_t nowMs, uint8_t maxTransactions = 1);
  /// Request cancellation without I2C.
  /// A confirmed or ambiguous conversion start enters bus-silent wait-idle
  /// reconciliation; the abandoned sample is never published or reused.
  /// Cancelling after any possible hardware effect sets hardwareConfigDirty() and
  /// moves configuration trust to UNKNOWN, so a verified replay through
  /// startRecover() or startApplyProfile() is required before the next typed read.
  /// @return Immediate disposition and whether reconciliation remains active.
  CancelDisposition cancelActiveOperation();
  /// Consume the pending terminal result exactly once by token without I2C.
  /// A token mismatch does not consume the result.
  /// @param token Token returned when the operation was accepted.
  /// @param[out] out Terminal operation result on success.
  /// @return OK, TOKEN_MISMATCH, or RESULT_NOT_AVAILABLE.
  Status takeResult(OperationToken token, OperationResult& out);
  /// Drop the binding and all local state without I2C.
  /// First cancel and poll any conversion through reconciliation, and call
  /// startShutdown() when a hardware idle request is required. Immediate reuse
  /// of the same device after abandoning an active conversion is caller-unsafe.
  void unbind();

  /// Compatibility synchronous initialization facade.
  ///
  /// Always performs one CONFIG probe, three writes, and three readbacks.
  /// ADS1115 has no ID register; this proves address reachability and profile
  /// plausibility only, with dynamic CONFIG OS/status bits masked out.
  /// If begin() fails after one or more writes may have reached hardware,
  /// hardwareConfigDirty() and hardwareConfigDirtyError() remain available even
  /// though the driver is not initialized. A later successful full apply clears
  /// the dirty diagnostic.
  /// @param config Transport callbacks, device address, timing, and conversion settings.
  /// @return Status::Ok() when the device responds and cached configuration is applied.
  Status begin(const Config& config);
  /// Compatibility wrapper around service() that discards its Status.
  /// @param nowMs Current monotonic time in milliseconds.
  void tick(uint32_t nowMs);
  /// Status-returning compatibility service step.
  /// Advances an active owner operation by at most one transaction, or services
  /// legacy conversion polling when no owner operation is active.
  /// @param nowMs Current monotonic time in milliseconds.
  /// @return Immediate status from the service step, or Status::Ok() when no
  /// I2C work is needed.
  Status service(uint32_t nowMs);
  /// Bus-silent compatibility alias for unbind().
  void end();

  /// Compatibility synchronous shutdown facade while keeping the binding.
  /// Config::nowMs is required when cached state is continuous, unknown, or
  /// dirty. A verified, clean single-shot profile retains the bounded clockless
  /// write/readback path; an unexpected OS-busy result then returns
  /// INVALID_CONFIG and requires explicit owner poll() reconciliation.
  /// Otherwise performs one CONFIG write and one or more bounded readiness
  /// readbacks; Config::cooperativeYield is called between timed polls.
  /// @return Final shutdown status or a bounded config/timeout/stalled-clock
  /// status. Post-write failure can require owner poll() reconciliation before
  /// another operation is accepted.
  Status shutdown();

  /// Check if begin() completed successfully and end() has not been called.
  /// @return true when the driver is initialized.
  bool isInitialized() const { return _initialized; }
  /// @return true when a valid transport/profile binding exists.
  bool isBound() const { return _bound; }
  /// @return Current hardware/profile trust state.
  ConfigurationState configurationState() const { return _configurationState; }
  /// @return Generation of the last verified profile commit.
  uint32_t configurationGeneration() const { return _configGeneration; }
  /// @return Active or unconsumed terminal operation token.
  OperationToken activeOperationToken() const { return _operationToken; }
  /// @return Active or unconsumed terminal operation kind.
  OperationKind operationKind() const { return _operationKind; }
  /// @return Current operation lifecycle state.
  OperationState operationState() const { return _operationState; }
  /// @return true when takeResult() can consume a terminal result.
  bool terminalResultAvailable() const { return _terminalResultAvailable; }
  /// Snapshot the last committed profile record and current trust state without I2C.
  /// @param[out] out Profile, state, and verified generation.
  /// @return OK when bound, or NOT_INITIALIZED when unbound.
  Status getAppliedProfile(AppliedProfileSnapshot& out) const;

  /// Get the cached configuration snapshot currently owned by the driver.
  /// The returned reference exposes the live transport callbacks and context;
  /// treat it as read-only diagnostics and never invoke them directly, or the
  /// driver's view of the device stops matching hardware.
  /// @return Cached driver configuration.
  const Config& getConfig() const { return _config; }

  /// @}

  /// @name Diagnostics and snapshots
  /// @{

  /// Probe ADS1115 CONFIG-register reachability without updating health counters.
  /// ADS1115 has no chip-ID register; this is a diagnostic I2C/register
  /// plausibility check, not identity proof.
  /// Requires a successfully initialized driver. Owner initialization and
  /// recovery instead use a tracked CONFIG read so their transport activity is
  /// reflected in health counters; public probe() remains diagnostic-only.
  /// Address NACK maps to DEVICE_NOT_FOUND; distinguishable timeout, bus, data
  /// NACK, and generic I2C failures are preserved.
  /// Transaction count: one CONFIG read.
  /// @return Status::Ok() when the CONFIG register can be read.
  Status probe();
  /// Attempt recovery from DEGRADED or OFFLINE state using tracked I2C.
  /// Transaction count: one CONFIG read, three writes, and three readbacks.
  /// @return Status::Ok() when the device responds and cached configuration is restored.
  Status recover();

  /// Populate a snapshot of cached configuration and runtime state without I2C.
  /// @param[out] out Snapshot to fill
  /// @return Status::Ok() always
  Status getSettings(SettingsSnapshot& out) const;

  /// @}

  /// @name Driver state and passive health
  /// @{

  /// @return Current coarse driver state.
  DriverState state() const { return _driverState; }
  /// @return Compatibility alias for state().
  DriverState driverState() const { return state(); }
  /// @return true when the driver is READY or DEGRADED.
  bool isOnline() const {
    return _driverState == DriverState::READY ||
           _driverState == DriverState::DEGRADED;
  }

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

  /// @}

  /// @name Conversion and compatibility jobs
  /// @{

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
  /// An asserted ALERT/RDY pin is accepted as an early readiness signal when the
  /// pin and conversion-ready thresholds are configured. Otherwise single-shot
  /// mode polls the OS bit after the conversion time and continuous mode tracks
  /// the configured data-rate interval between fresh samples.
  /// Requires a time source: either Config::nowMs, or an external
  /// tick(nowMs)/service(nowMs) timebase. Without one the elapsed interval stays
  /// zero and readiness never becomes true.
  /// Transaction count: zero when the cached, timing, or ALERT path is enough;
  /// otherwise one CONFIG read in single-shot OS-bit polling.
  /// @param[out] ready true when conversion data can be read
  /// @return Status from the readiness path
  Status readConversionReady(bool& ready);

  /// Read the conversion register as a signed two's-complement sample.
  /// In continuous mode this returns the latest register value immediately and
  /// does not wait for a fresh data-rate interval. Use readConversionReady()
  /// first when the caller requires a fresh continuous sample indication.
  /// Transaction count: one conversion-register read, plus the CONFIG read that
  /// readConversionReady() may perform first in single-shot mode.
  /// @param[out] out Signed conversion code.
  /// @return Status::Ok() on a successful register read.
  Status readRaw(int16_t& out);

  /// Read a signed sample and scale it using the active gain.
  /// Requires a verified, clean, single-shot configuration. Direct diagnostic
  /// mutations return Err::CONFIG_UNKNOWN until a full apply/recover succeeds.
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

  /// Perform a fresh single-shot conversion with a finite deadline.
  /// Requires a verified, clean, single-shot configuration. This hardened
  /// compatibility path does not support continuous mode.
  /// Requires Config::nowMs; returns INVALID_CONFIG before starting conversion
  /// when no monotonic clock hook is configured.
  /// Use readLatestRaw() when the caller intentionally wants the current
  /// continuous-mode register value immediately.
  /// Transaction count: one CONFIG write to start, one conversion-register read,
  /// and one CONFIG readback per readiness poll. A device still reporting OS busy
  /// after the worst-case conversion interval is re-polled at one eighth of the
  /// worst-case conversion interval (rounded up to milliseconds) until the
  /// deadline. Worst-case wall time is bounded by timeoutMs plus
  /// bus-silent post-callback reconciliation after an ambiguous start.
  /// A timeout or stalled clock can leave post-write reconciliation active;
  /// while operationState() is RECONCILING, drive poll() with advancing time and
  /// consume the terminal result through activeOperationToken(). The stopped-
  /// clock guard permits 100000 same-tick polls, suitable for an approximately
  /// one-millisecond monotonic source but not a substitute for a real deadline.
  /// @param[out] out Signed conversion code.
  /// @param timeoutMs Maximum wait in milliseconds.
  /// @return Status::Ok() on success, Err::TIMEOUT when the deadline expires,
  /// or Err::CLOCK_STALLED when the supplied clock stops advancing.
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);

  /// Blocking read with voltage scaling.
  /// Requires Config::nowMs plus the verified, clean, single-shot contract and
  /// possible post-error reconciliation cleanup described by readBlocking().
  /// @param[out] volts Converted input voltage.
  /// @param timeoutMs Maximum wait in milliseconds.
  /// @return Status::Ok() on success, Err::TIMEOUT when the deadline expires,
  /// or Err::CLOCK_STALLED when the supplied clock stops advancing.
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);

  /// Start a poll-chunked single-shot conversion job without performing I2C.
  /// While any poll-chunked job is active, normal public I2C/configuration APIs
  /// return Err::BUSY; use the matching poll method or cancelJob().
  /// Use pollSingleShot() to advance the job with an explicit transaction budget.
  /// A terminal poll result remains pending; call takeResult(result.token, ...)
  /// before starting another operation.
  /// @return Err::IN_PROGRESS when scheduled; INVALID_CONFIG when the legacy
  /// clock hook is absent; otherwise the startRead() validation/precondition
  /// status. No I2C is performed.
  Status startSingleShot();

  /// Start a poll-chunked single-shot conversion job for a mux without I2C.
  /// @param mux Input mux to use for this conversion.
  /// @return Err::IN_PROGRESS when scheduled; INVALID_PARAM for an invalid mux,
  /// INVALID_CONFIG when the legacy clock hook is absent, or the startRead()
  /// initialization, busy, mode, and configuration-trust precondition status.
  /// No I2C is performed. Consume the terminal token through takeResult().
  Status startSingleShot(Mux mux);

  /// Advance a single-shot job by at most maxInstructions transport callbacks.
  /// maxInstructions is clamped to 3; passing 0 performs no transport work.
  /// Delay gates return with instructionsUsed == 0.
  /// On a terminal result, consume the result token with takeResult() before
  /// starting another operation.
  /// @param nowMs Current monotonic time in milliseconds.
  /// @param maxInstructions Maximum transport callbacks to perform this poll.
  /// @return Job progress, terminal status, and callbacks consumed. An idle
  /// matching facade returns RESULT_NOT_AVAILABLE without performing I2C.
  PollResult pollSingleShot(uint32_t nowMs, uint8_t maxInstructions = 1);

  /// Start a staged cached-config apply job without performing I2C.
  /// While any poll-chunked job is active, normal public I2C/configuration APIs
  /// return Err::BUSY; use the matching poll method or cancelJob().
  /// Normal continuous-mode background conversion state is allowed; an active
  /// single-shot conversion is rejected with Err::BUSY.
  /// The job writes low threshold, high threshold, CONFIG, then always reads
  /// back all three registers before committing the applied profile.
  /// A terminal poll result remains pending; call takeResult(result.token, ...)
  /// before starting another operation.
  /// @return Err::IN_PROGRESS when scheduled; INVALID_CONFIG when the legacy
  /// clock hook is absent; otherwise the startApplyProfile() validation or
  /// precondition status. No I2C is performed.
  Status startApplyConfigJob();

  /// Advance a config-apply job by at most maxInstructions transport callbacks.
  /// maxInstructions is clamped to 3; passing 0 performs no transport work.
  /// @param nowMs Current monotonic time in milliseconds. It drives the operation
  ///        deadline, the per-callback timeout partition, and health timestamps.
  /// @param maxInstructions Maximum transport callbacks to perform this poll.
  /// @return Job progress, terminal status, and callbacks consumed. An idle
  /// matching facade returns RESULT_NOT_AVAILABLE without performing I2C.
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

  /// @}

  /// @name Typed configuration diagnostics
  /// @{

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

  /// @}

  /// @name Raw register diagnostics
  /// @{

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
  /// If the transport reports an ambiguous error after the raw write is
  /// attempted, that Status becomes the dirty diagnostic because hardware may
  /// have accepted the write. A definite address NACK cannot have changed the
  /// target and does not dirty previously clean state.
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

  /// @}

  /// @name Comparator and ALERT/RDY diagnostics
  /// @{

  /// Set signed comparator thresholds. Cache changes commit after both writes succeed.
  /// Thresholds are signed raw conversion codes and must be recalculated if the
  /// gain/full-scale range changes. high must exceed low, so this cannot program
  /// the datasheet conversion-ready pattern; use enableConversionReadyPin() for
  /// that. If the second write fails after the first reached hardware,
  /// hardwareConfigDirty() is set with the original error.
  /// Transaction count: two threshold writes.
  /// @param low Low threshold raw code.
  /// @param high High threshold raw code.
  /// @return Status::Ok() when both threshold registers are written.
  Status setThresholds(int16_t low, int16_t high);

  /// Read signed comparator thresholds without changing the desired or cached
  /// applied profile.
  /// Matching values preserve an existing clean VERIFIED profile. A mismatch
  /// invalidates full-profile trust until a complete apply/recover succeeds.
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
  /// Transaction count: three writes and three read-back reads.
  /// @return Status::Ok() when thresholds and CONFIG are written.
  Status enableConversionReadyPin();

  /// Disable comparator output by setting queue to DISABLE.
  /// Threshold registers are left as programmed, so set them explicitly before
  /// re-enabling the comparator.
  /// Transaction count: one CONFIG write.
  /// @return Status::Ok() when CONFIG was written.
  Status disableComparator();

  /// @}

  /// @name Conversion utilities
  /// @{

  /// @param raw Signed conversion code.
  /// @return Input voltage using the cached PGA range.
  float rawToVoltage(int16_t raw) const;
  /// @return LSB size in volts for the cached PGA range.
  float getLsbVoltage() const;
  /// @return Conservative conversion time in milliseconds for the cached data rate.
  uint32_t getConversionTimeMs() const;

  /// @}

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
  Status _applyCachedConfigSynchronously();
  Status _writeConfigOnly();
  void _replaceHardwareConfigDirty(const Status& st);
  void _markHardwareConfigDirtyIfClean(const Status& st);
  void _clearHardwareConfigDirty();
  uint16_t _buildConfigRegister() const;
  uint32_t _nowMs() const;
  void _cooperativeYield() const;
  Status _jobBusyStatus() const;
  uint8_t _instructionBudget(uint8_t maxInstructions) const;
  PollResult _pollResult(Status status, uint8_t instructionsUsed, bool done) const;
  PollResult _finishOperation(const Status& status, OperationState state,
                              uint8_t transactionsUsed, bool sampleValid = false);
  PollResult _abandonConversion(const Status& reason, OperationState terminalState,
                                uint8_t transactionsUsed);
  Status _beginOperation(OperationKind kind, uint32_t nowMs, uint32_t deadlineMs,
                         OperationToken& token);
  bool _deadlineReached(uint32_t nowMs) const;
  bool _singleShotMayBeActive() const;
  DataRate _operationGuardDataRate() const;
  Status _activeHardwareBusyStatus() const;
  void _resetOperationScratch();
  void _loadProfileIntoConfig(const DeviceProfile& profile);
  DeviceProfile _profileFromConfig() const;
  uint16_t _buildConfigRegisterFor(const DeviceProfile& profile, Mux mux,
                                   Gain gain) const;
  Status _verifyJobReadback(uint8_t reg, uint16_t expected, const char* message,
                            uint16_t* observedOut = nullptr);

  // === State ===
  static constexpr uint8_t kMaxJobInstructions = 3;
  static constexpr uint32_t kMaxSameTickPolls = 100000U;
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
  ChannelRequest _channelRequest{};
  bool _jobStartWriteAttempted = false;
  bool _jobAnyWriteConfirmed = false;
  uint32_t _jobNextReadyPollMs = 0;
  uint32_t _jobWaitDurationMs = 0;
  bool _jobWaitStartPending = false;
  bool _shutdownWaitForIdle = false;
  Status _abandonStatus = Status::Ok();
  OperationState _abandonTerminalState = OperationState::FAILED;
  bool _abandonWaitStartPending = false;
  uint32_t _abandonWaitStartMs = 0;

  // === Owner Operation State ===
  OperationKind _operationKind = OperationKind::NONE;
  OperationState _operationState = OperationState::IDLE;
  OperationToken _operationToken{};
  uint32_t _nextOperationToken = 1;
  uint32_t _operationDeadlineMs = 0;
  uint32_t _pollNowMs = 0;
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
  bool _conversionStartMsValid = false;
  uint8_t _continuousSettlePeriods = 1;
  int16_t _lastRawValue = 0;
};

} // namespace ADS1115
