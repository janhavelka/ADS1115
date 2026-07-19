/// @file ADS1115.cpp
/// @brief Implementation of ADS1115 driver

#include "ADS1115/ADS1115.h"

#include <climits>

namespace ADS1115 {

namespace {

constexpr uint8_t kMinAddress = 0x48;
constexpr uint8_t kMaxAddress = 0x4B;
constexpr uint16_t kConfigReadbackMask =
    cmd::MASK_MUX | cmd::MASK_PGA | cmd::MASK_MODE | cmd::MASK_DR |
    cmd::MASK_COMP_MODE | cmd::MASK_COMP_POL | cmd::MASK_COMP_LAT |
    cmd::MASK_COMP_QUE;

bool isValidMux(Mux mux) {
  return static_cast<uint8_t>(mux) <= static_cast<uint8_t>(Mux::AIN3_GND);
}

bool isValidGain(Gain gain) {
  return static_cast<uint8_t>(gain) <= static_cast<uint8_t>(Gain::FSR_0_256V);
}

Gain decodePgaBits(uint8_t pga) {
  if (pga >= static_cast<uint8_t>(Gain::FSR_0_256V)) {
    return Gain::FSR_0_256V;
  }
  return static_cast<Gain>(pga);
}

bool isValidDataRate(DataRate rate) {
  return static_cast<uint8_t>(rate) <= static_cast<uint8_t>(DataRate::SPS_860);
}

bool isValidMode(Mode mode) {
  return static_cast<uint8_t>(mode) <= static_cast<uint8_t>(Mode::SINGLE_SHOT);
}

bool isValidCompMode(ComparatorMode mode) {
  return static_cast<uint8_t>(mode) <= static_cast<uint8_t>(ComparatorMode::WINDOW);
}

bool isValidCompPolarity(ComparatorPolarity polarity) {
  return static_cast<uint8_t>(polarity) <= static_cast<uint8_t>(ComparatorPolarity::ACTIVE_HIGH);
}

bool isValidCompLatch(ComparatorLatch latch) {
  return static_cast<uint8_t>(latch) <= static_cast<uint8_t>(ComparatorLatch::LATCHING);
}

bool isValidCompQueue(ComparatorQueue queue) {
  return static_cast<uint8_t>(queue) <= static_cast<uint8_t>(ComparatorQueue::DISABLE);
}

bool isAlertRdyModeConfigured(const Config& cfg) {
  constexpr int16_t kAlertRdyLow = 0;
  constexpr int16_t kAlertRdyHigh = -32768;  // 0x8000 as int16_t
  return cfg.compThresholdLow == kAlertRdyLow &&
         cfg.compThresholdHigh == kAlertRdyHigh &&
         cfg.compQueue == ComparatorQueue::ASSERT_1 &&
         cfg.compMode == ComparatorMode::TRADITIONAL &&
         cfg.compLatch == ComparatorLatch::NON_LATCHING;
}

bool isAlertRdyPinConfigured(const Config& cfg) {
  return cfg.alertRdyPin >= 0 && cfg.gpioRead != nullptr;
}

bool useAlertRdyPin(const Config& cfg) {
  return isAlertRdyPinConfigured(cfg) && isAlertRdyModeConfigured(cfg);
}

bool isAlertRdyAsserted(const Config& cfg) {
  if (!useAlertRdyPin(cfg)) {
    return false;
  }
  bool level = cfg.gpioRead(cfg.alertRdyPin, cfg.gpioUser);
  if (cfg.compPolarity == ComparatorPolarity::ACTIVE_HIGH) {
    return level;
  }
  return !level;
}

bool isValidConfigValue(uint16_t config) {
  uint8_t mux = static_cast<uint8_t>((config & cmd::MASK_MUX) >> cmd::BIT_MUX);
  uint8_t pga = static_cast<uint8_t>((config & cmd::MASK_PGA) >> cmd::BIT_PGA);
  uint8_t mode = static_cast<uint8_t>((config & cmd::MASK_MODE) >> cmd::BIT_MODE);
  uint8_t dr = static_cast<uint8_t>((config & cmd::MASK_DR) >> cmd::BIT_DR);
  uint8_t compMode = static_cast<uint8_t>((config & cmd::MASK_COMP_MODE) >> cmd::BIT_COMP_MODE);
  uint8_t compPol = static_cast<uint8_t>((config & cmd::MASK_COMP_POL) >> cmd::BIT_COMP_POL);
  uint8_t compLat = static_cast<uint8_t>((config & cmd::MASK_COMP_LAT) >> cmd::BIT_COMP_LAT);
  uint8_t compQue = static_cast<uint8_t>((config & cmd::MASK_COMP_QUE) >> cmd::BIT_COMP_QUE);

  return mux <= 7 && pga <= 7 && mode <= 1 && dr <= 7 &&
         compMode <= 1 && compPol <= 1 && compLat <= 1 && compQue <= 3;
}

bool isValidRegister(uint8_t reg) {
  return reg <= cmd::REG_HI_THRESH;
}

bool isWritableRegister(uint8_t reg) {
  return reg >= cmd::REG_CONFIG && reg <= cmd::REG_HI_THRESH;
}

bool isDefiniteAddressAbsence(Err err) {
  return err == Err::I2C_NACK_ADDR;
}

bool isUncertainWriteFailure(Err err) {
  return err == Err::I2C_ERROR ||
         err == Err::TIMEOUT ||
         err == Err::I2C_NACK_DATA ||
         err == Err::I2C_TIMEOUT ||
         err == Err::I2C_BUS;
}

bool elapsedAtLeast(uint32_t startMs, uint32_t intervalMs, uint32_t nowMs) {
  return (nowMs - startMs) >= intervalMs;
}

} // namespace

uint16_t dataRateSps(DataRate rate) {
  static constexpr uint16_t kRates[] = {8, 16, 32, 64, 128, 250, 475, 860};
  const uint8_t index = static_cast<uint8_t>(rate);
  return index < (sizeof(kRates) / sizeof(kRates[0])) ? kRates[index] : 0;
}

uint32_t worstCaseConversionTimeUs(DataRate rate) {
  const uint16_t sps = dataRateSps(rate);
  if (sps == 0) {
    return 0;
  }
  // Slow datasheet tolerance is -10%. One millisecond guard covers integer
  // rounding and scheduling at the conversion boundary.
  const uint32_t slowPeriodUs =
      (10000000UL + (9UL * sps) - 1UL) / (9UL * sps);
  return slowPeriodUs + 1000UL;
}

int32_t gainFullScaleMicrovolts(Gain gain) {
  static constexpr int32_t kFullScaleUv[] = {
      6144000, 4096000, 2048000, 1024000, 512000, 256000};
  const uint8_t index = static_cast<uint8_t>(gain);
  return index < (sizeof(kFullScaleUv) / sizeof(kFullScaleUv[0]))
             ? kFullScaleUv[index]
             : 0;
}

Status rawToMicrovolts(int16_t raw, Gain gain, int32_t& out) {
  const int32_t fullScaleUv = gainFullScaleMicrovolts(gain);
  if (fullScaleUv == 0) {
    out = 0;
    return Status::Error(Err::INVALID_PARAM, "Invalid gain");
  }
  int64_t numerator = static_cast<int64_t>(raw) * fullScaleUv;
  numerator += (numerator >= 0) ? 16384 : -16384;
  out = static_cast<int32_t>(numerator / 32768);
  return Status::Ok();
}

bool isSingleEnded(Mux mux) {
  return isValidMux(mux) &&
         static_cast<uint8_t>(mux) >= static_cast<uint8_t>(Mux::AIN0_GND);
}

int8_t positiveInput(Mux mux) {
  static constexpr int8_t kPositive[] = {0, 0, 1, 2, 0, 1, 2, 3};
  const uint8_t index = static_cast<uint8_t>(mux);
  return index < (sizeof(kPositive) / sizeof(kPositive[0])) ? kPositive[index] : -1;
}

int8_t negativeInput(Mux mux) {
  static constexpr int8_t kNegative[] = {1, 3, 3, 3, -1, -1, -1, -1};
  const uint8_t index = static_cast<uint8_t>(mux);
  return index < (sizeof(kNegative) / sizeof(kNegative[0])) ? kNegative[index] : -2;
}

Status validateComparatorProfile(const ComparatorProfile& profile) {
  if (static_cast<uint8_t>(profile.use) >
          static_cast<uint8_t>(ComparatorUse::CONVERSION_READY) ||
      !isValidCompMode(profile.mode) ||
      !isValidCompPolarity(profile.polarity) ||
      !isValidCompLatch(profile.latch) || !isValidCompQueue(profile.queue)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid comparator profile");
  }
  if (profile.use == ComparatorUse::OFF) {
    return profile.queue == ComparatorQueue::DISABLE
               ? Status::Ok()
               : Status::Error(Err::INVALID_CONFIG, "Disabled comparator queue must be disabled");
  }
  if (profile.use == ComparatorUse::CONVERSION_READY) {
    const bool validReadyPattern =
        profile.mode == ComparatorMode::TRADITIONAL &&
        profile.latch == ComparatorLatch::NON_LATCHING &&
        profile.queue == ComparatorQueue::ASSERT_1 &&
        profile.lowThreshold == 0 &&
        profile.highThreshold == static_cast<int16_t>(0x8000);
    return validReadyPattern
               ? Status::Ok()
               : Status::Error(Err::INVALID_CONFIG, "Invalid conversion-ready profile");
  }
  if (profile.queue == ComparatorQueue::DISABLE ||
      profile.highThreshold <= profile.lowThreshold) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid comparator thresholds");
  }
  return Status::Ok();
}

Status validateDeviceProfile(const DeviceProfile& profile) {
  if (profile.i2cAddress < kMinAddress || profile.i2cAddress > kMaxAddress ||
      !isValidMux(profile.defaultMux) || !isValidGain(profile.defaultGain) ||
      !isValidDataRate(profile.dataRate) || !isValidMode(profile.mode)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid device profile");
  }
  return validateComparatorProfile(profile.comparator);
}

Status validateChannelRequest(const ChannelRequest& request) {
  if (!isValidMux(request.mux) || !isValidGain(request.gain)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid channel request");
  }
  return Status::Ok();
}

uint32_t operationDeadlineMs(uint8_t channelCount, DataRate rate,
                             uint32_t schedulingMarginMs) {
  if (channelCount == 0) {
    return 0;
  }
  const uint32_t conversionUs = worstCaseConversionTimeUs(rate);
  if (conversionUs == 0) {
    return 0;
  }
  const uint64_t conversionMs = (static_cast<uint64_t>(conversionUs) + 999ULL) / 1000ULL;
  const uint64_t total = conversionMs * channelCount + schedulingMarginMs;
  return total > static_cast<uint64_t>(INT32_MAX)
             ? static_cast<uint32_t>(INT32_MAX)
             : static_cast<uint32_t>(total);
}

// ============================================================================
// Lifecycle
// ============================================================================

Status ADS1115::bind(const DriverConfig& driverConfig, const DeviceProfile& profile) {
  if (driverConfig.i2cWrite == nullptr || driverConfig.i2cWriteRead == nullptr ||
      driverConfig.transferTimeoutMs == 0 ||
      driverConfig.transferTimeoutMs > static_cast<uint32_t>(INT32_MAX)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid transport binding");
  }
  Status st = validateDeviceProfile(profile);
  if (!st.ok()) {
    return st;
  }
  if (_jobActive || _terminalResultAvailable) {
    return Status::Error(Err::BUSY, "Finish active binding operation before rebind");
  }

  unbind();
  _driverConfig = driverConfig;
  _desiredProfile = profile;
  _candidateProfile = profile;
  _config.i2cWrite = driverConfig.i2cWrite;
  _config.i2cWriteRead = driverConfig.i2cWriteRead;
  _config.i2cUser = driverConfig.i2cUser;
  _config.i2cTimeoutMs = driverConfig.transferTimeoutMs;
  _config.strictInitVerify = true;
  _loadProfileIntoConfig(profile);
  _bound = true;
  _configurationState = ConfigurationState::UNCONFIGURED;
  return Status::Ok();
}

void ADS1115::unbind() {
  _config = Config{};
  _driverConfig = DriverConfig{};
  _desiredProfile = DeviceProfile{};
  _appliedProfile = DeviceProfile{};
  _candidateProfile = DeviceProfile{};
  _bound = false;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _hardwareConfigDirty = false;
  _hardwareConfigDirtyError = Status::Ok();
  _hardwareConfigDirtyAddress = kInvalidDirtyAddress;
  _configurationState = ConfigurationState::UNBOUND;
  _configurationStateBeforeOperation = ConfigurationState::UNBOUND;
  _configGeneration = 0;
  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _continuousSettlePeriods = 1;
  _lastRawValue = 0;
  _sampleSequence = 0;
  _terminalResultAvailable = false;
  _terminalResult = OperationResult{};
  _operationKind = OperationKind::NONE;
  _operationState = OperationState::IDLE;
  _operationToken = OperationToken{};
  _operationStartMs = 0;
  _operationDeadlineMs = 0;
  _activeTransferTimeoutMs = 0;
  _resetOperationScratch();
}

Status ADS1115::_beginOperation(OperationKind kind, uint32_t nowMs,
                                uint32_t deadlineMs, OperationToken& token) {
  token = OperationToken{};
  if (!_bound) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not bound");
  }
  if (_jobActive) {
    return Status::Error(Err::BUSY, "Operation already active");
  }
  if (_terminalResultAvailable) {
    return Status::Error(Err::BUSY, "Take terminal result before starting another operation");
  }
  if (static_cast<int32_t>(deadlineMs - nowMs) <= 0) {
    return Status::Error(Err::INVALID_PARAM, "Deadline must be in the future");
  }

  _resetOperationScratch();
  _operationKind = kind;
  _operationState = OperationState::ACTIVE;
  _operationStartMs = nowMs;
  _operationDeadlineMs = deadlineMs;
  _activeTransferTimeoutMs = _driverConfig.transferTimeoutMs;
  _operationToken.value = _nextOperationToken++;
  if (_nextOperationToken == 0) {
    _nextOperationToken = 1;
  }
  token = _operationToken;
  _jobActive = true;
  _lastJobStatus = Status{Err::IN_PROGRESS, 0, "Operation started"};
  _configurationStateBeforeOperation = _configurationState;
  return _lastJobStatus;
}

Status ADS1115::startInitialize(uint32_t nowMs, uint32_t deadlineMs,
                                OperationToken& token) {
  if (!_bound) {
    token = OperationToken{};
    return Status::Error(Err::NOT_INITIALIZED, "Driver not bound");
  }
  Status st = _beginOperation(OperationKind::INITIALIZE, nowMs, deadlineMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  _candidateProfile = _desiredProfile;
  _configurationState = ConfigurationState::APPLYING;
  _jobState = JobState::PROBE_CONFIG;
  return st;
}

Status ADS1115::startApplyProfile(const DeviceProfile& profile, uint32_t nowMs,
                                  uint32_t deadlineMs, OperationToken& token) {
  token = OperationToken{};
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = validateDeviceProfile(profile);
  if (!st.ok()) {
    return st;
  }
  if (profile.i2cAddress != _config.i2cAddress) {
    return Status::Error(Err::INVALID_PARAM, "Address change requires rebind");
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  st = _beginOperation(OperationKind::APPLY_PROFILE, nowMs, deadlineMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  _candidateProfile = profile;
  _configurationState = ConfigurationState::APPLYING;
  _jobThresholdLow = profile.comparator.lowThreshold;
  _jobThresholdHigh = profile.comparator.highThreshold;
  _jobConfigRegister = _buildConfigRegisterFor(profile, profile.defaultMux,
                                                profile.defaultGain);
  _jobState = JobState::APPLY_WRITE_LOW_THRESHOLD;
  return st;
}

Status ADS1115::startRecover(uint32_t nowMs, uint32_t deadlineMs,
                             OperationToken& token) {
  token = OperationToken{};
  if (!_bound) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not bound");
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  Status st = _beginOperation(OperationKind::RECOVER, nowMs, deadlineMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  _candidateProfile = _desiredProfile;
  _configurationState = ConfigurationState::APPLYING;
  _jobState = JobState::PROBE_CONFIG;
  return st;
}

Status ADS1115::startRead(const ChannelRequest& request, uint32_t nowMs,
                          uint32_t deadlineMs, OperationToken& token) {
  token = OperationToken{};
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  Status st = validateChannelRequest(request);
  if (!st.ok()) {
    return st;
  }
  if (_desiredProfile.mode != Mode::SINGLE_SHOT) {
    return Status::Error(Err::UNSUPPORTED_OPERATION,
                         "Owner-safe reads require single-shot mode");
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  if (_configurationState != ConfigurationState::VERIFIED ||
      _hardwareConfigDirty) {
    return Status::Error(Err::CONFIG_UNKNOWN,
                         "Recover verified configuration before typed read");
  }
  st = _beginOperation(OperationKind::READ_SINGLE_SHOT, nowMs, deadlineMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  _channelRequest = request;
  _jobMux = request.mux;
  _jobGain = request.gain;
  _jobConfigRegister = _buildConfigRegisterFor(_desiredProfile, request.mux,
                                                request.gain) | cmd::OS_START;
  _jobState = JobState::SINGLE_SHOT_WRITE_CONFIG;
  _configurationState = ConfigurationState::APPLYING;
  return st;
}

Status ADS1115::startShutdown(uint32_t nowMs, uint32_t deadlineMs,
                              OperationToken& token) {
  token = OperationToken{};
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  Status st = _beginOperation(OperationKind::SHUTDOWN, nowMs, deadlineMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  _jobConfigRegister = _buildConfigRegisterFor(_desiredProfile,
                                                _config.mux, _config.gain);
  _jobConfigRegister |= cmd::MASK_MODE;
  _jobState = JobState::SHUTDOWN_WRITE_CONFIG;
  return st;
}

PollResult ADS1115::poll(uint32_t nowMs, uint8_t maxTransactions) {
  if (!_bound) {
    return _pollResult(Status::Error(Err::NOT_INITIALIZED, "Driver not bound"), 0, true);
  }
  if (!_jobActive) {
    if (_terminalResultAvailable) {
      return _pollResult(_terminalResult.status, 0, true);
    }
    return _pollResult(Status::Ok(), 0, true);
  }

  const uint8_t budget = _instructionBudget(maxTransactions);
  uint8_t used = 0;

  const int32_t remainingMs = static_cast<int32_t>(_operationDeadlineMs - nowMs);
  if (remainingMs > 0) {
    const uint32_t remaining = static_cast<uint32_t>(remainingMs);
    _activeTransferTimeoutMs = remaining < _driverConfig.transferTimeoutMs
                                   ? remaining
                                   : _driverConfig.transferTimeoutMs;
  }

  if (_deadlineReached(nowMs) && _jobState != JobState::WAIT_IDLE_AFTER_ABANDON) {
    const Status timeout = Status::Error(Err::TIMEOUT, "Operation deadline reached");
    if (_operationKind == OperationKind::READ_SINGLE_SHOT &&
        _jobStartWriteAttempted) {
      if (!_conversionStarted &&
          _jobState == JobState::SINGLE_SHOT_READ_CONVERSION) {
        return _finishOperation(timeout, OperationState::TIMED_OUT, 0);
      }
      _abandonStatus = timeout;
      _operationState = OperationState::RECONCILING;
      _jobState = JobState::WAIT_IDLE_AFTER_ABANDON;
      _configurationState = ConfigurationState::UNKNOWN;
      _markHardwareConfigDirty(timeout);
      return _pollResult(timeout, 0, false);
    }
    if (_jobAnyWriteConfirmed || _jobStartWriteAttempted) {
      _configurationState = ConfigurationState::UNKNOWN;
      _markHardwareConfigDirty(timeout);
    } else {
      _configurationState = _configurationStateBeforeOperation;
    }
    return _finishOperation(timeout, OperationState::TIMED_OUT, 0);
  }

  while (true) {
    switch (_jobState) {
      case JobState::PROBE_CONFIG: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st;
        if (_operationKind == OperationKind::INITIALIZE) {
          st = _probeRaw();
        } else {
          uint16_t configReg = 0;
          st = _readRegister16Tracked(cmd::REG_CONFIG, configReg);
        }
        used++;
        if (!st.ok()) {
          _configurationState = _configurationStateBeforeOperation;
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobThresholdLow = _candidateProfile.comparator.lowThreshold;
        _jobThresholdHigh = _candidateProfile.comparator.highThreshold;
        _jobConfigRegister = _buildConfigRegisterFor(
            _candidateProfile, _candidateProfile.defaultMux,
            _candidateProfile.defaultGain);
        _jobState = JobState::APPLY_WRITE_LOW_THRESHOLD;
        continue;
      }

      case JobState::APPLY_WRITE_LOW_THRESHOLD: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _writeRegister16Tracked(cmd::REG_LO_THRESH,
                                            static_cast<uint16_t>(_jobThresholdLow));
        used++;
        if (!st.ok()) {
          if (isUncertainWriteFailure(st.code)) {
            _configurationState = ConfigurationState::UNKNOWN;
            _markHardwareConfigDirtyIfClean(st);
          } else {
            _configurationState = _configurationStateBeforeOperation;
          }
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobAnyWriteConfirmed = true;
        _jobState = JobState::APPLY_WRITE_HIGH_THRESHOLD;
        continue;
      }

      case JobState::APPLY_WRITE_HIGH_THRESHOLD: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _writeRegister16Tracked(cmd::REG_HI_THRESH,
                                            static_cast<uint16_t>(_jobThresholdHigh));
        used++;
        if (!st.ok()) {
          _configurationState = ConfigurationState::UNKNOWN;
          _markHardwareConfigDirty(st);
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobAnyWriteConfirmed = true;
        _jobState = JobState::APPLY_WRITE_CONFIG;
        continue;
      }

      case JobState::APPLY_WRITE_CONFIG: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _writeRegister16Tracked(cmd::REG_CONFIG, _jobConfigRegister);
        used++;
        if (!st.ok()) {
          _configurationState = ConfigurationState::UNKNOWN;
          _markHardwareConfigDirty(st);
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobAnyWriteConfirmed = true;
        _jobState = JobState::APPLY_VERIFY_LOW_THRESHOLD;
        continue;
      }

      case JobState::APPLY_VERIFY_LOW_THRESHOLD: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _verifyJobReadback(cmd::REG_LO_THRESH,
                                       static_cast<uint16_t>(_jobThresholdLow),
                                       "Low threshold readback mismatch");
        used++;
        if (!st.ok()) {
          _configurationState = ConfigurationState::UNKNOWN;
          _markHardwareConfigDirty(st);
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobState = JobState::APPLY_VERIFY_HIGH_THRESHOLD;
        continue;
      }

      case JobState::APPLY_VERIFY_HIGH_THRESHOLD: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _verifyJobReadback(cmd::REG_HI_THRESH,
                                       static_cast<uint16_t>(_jobThresholdHigh),
                                       "High threshold readback mismatch");
        used++;
        if (!st.ok()) {
          _configurationState = ConfigurationState::UNKNOWN;
          _markHardwareConfigDirty(st);
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobState = JobState::APPLY_VERIFY_CONFIG;
        continue;
      }

      case JobState::APPLY_VERIFY_CONFIG: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _verifyJobReadback(cmd::REG_CONFIG, _jobConfigRegister,
                                       "Config readback mismatch");
        used++;
        if (!st.ok()) {
          _configurationState = ConfigurationState::UNKNOWN;
          _markHardwareConfigDirty(st);
          return _finishOperation(st, OperationState::FAILED, used);
        }
        if (_operationKind == OperationKind::SHUTDOWN) {
          _config.mode = Mode::SINGLE_SHOT;
          _appliedProfile.mode = Mode::SINGLE_SHOT;
          if (_configurationStateBeforeOperation == ConfigurationState::VERIFIED &&
              !_hardwareConfigDirty) {
            _configurationState = ConfigurationState::VERIFIED;
          } else {
            _configurationState = ConfigurationState::UNKNOWN;
          }
        } else {
          _desiredProfile = _candidateProfile;
          _appliedProfile = _candidateProfile;
          _loadProfileIntoConfig(_candidateProfile);
          _configurationState = ConfigurationState::VERIFIED;
          _clearHardwareConfigDirty();
        }
        _initialized = true;
        _driverState = DriverState::READY;
        if (_configurationState == ConfigurationState::VERIFIED) {
          _configGeneration++;
          if (_configGeneration == 0) {
            _configGeneration = 1;
          }
        }
        _conversionStarted = (_config.mode == Mode::CONTINUOUS);
        _continuousSettlePeriods =
            (_config.mode == Mode::CONTINUOUS &&
             _operationKind != OperationKind::INITIALIZE)
                ? 2
                : 1;
        _conversionReady = false;
        _conversionStartMs = nowMs;
        return _finishOperation(Status::Ok(), OperationState::SUCCEEDED, used);
      }

      case JobState::SINGLE_SHOT_WRITE_CONFIG: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        _jobStartWriteAttempted = true;
        _conversionStartMs = nowMs;
        Status st = _writeRegister16Tracked(cmd::REG_CONFIG, _jobConfigRegister);
        used++;
        if (!st.ok()) {
          if (isUncertainWriteFailure(st.code)) {
            _abandonStatus = st;
            _operationState = OperationState::RECONCILING;
            _jobState = JobState::WAIT_IDLE_AFTER_ABANDON;
            _conversionStarted = true;
            _conversionReady = false;
            _configurationState = ConfigurationState::UNKNOWN;
            _markHardwareConfigDirtyIfClean(st);
            return _pollResult(st, used, false);
          }
          _configurationState = _configurationStateBeforeOperation;
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobAnyWriteConfirmed = true;
        _conversionStarted = true;
        _conversionReady = false;
        const uint32_t conversionMs =
            (worstCaseConversionTimeUs(_desiredProfile.dataRate) + 999UL) / 1000UL;
        _jobNextReadyPollMs = nowMs + conversionMs;
        _jobState = JobState::SINGLE_SHOT_WAIT_CONVERSION;
        return _pollResult(_lastJobStatus, used, false);
      }

      case JobState::SINGLE_SHOT_WAIT_CONVERSION:
        if (static_cast<int32_t>(nowMs - _jobNextReadyPollMs) < 0) {
          return _pollResult(_lastJobStatus, used, false);
        }
        _jobState = JobState::SINGLE_SHOT_POLL_READY;
        continue;

      case JobState::SINGLE_SHOT_POLL_READY: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        uint16_t configReg = 0;
        Status st = _readRegister16Tracked(cmd::REG_CONFIG, configReg);
        used++;
        if (!st.ok()) {
          _configurationState = ConfigurationState::UNKNOWN;
          return _finishOperation(st, OperationState::FAILED, used);
        }
        if ((configReg & kConfigReadbackMask) !=
            (_jobConfigRegister & kConfigReadbackMask)) {
          st = Status::Error(Err::READBACK_MISMATCH,
                             "Read profile mismatch", configReg);
          _configurationState = ConfigurationState::UNKNOWN;
          _markHardwareConfigDirty(st);
          return _finishOperation(st, OperationState::FAILED, used);
        }
        if ((configReg & cmd::MASK_OS) != cmd::OS_IDLE) {
          _jobNextReadyPollMs = nowMs + 1U;
          _jobState = JobState::SINGLE_SHOT_WAIT_CONVERSION;
          return _pollResult(_lastJobStatus, used, false);
        }
        _config.mux = _jobMux;
        _config.gain = _jobGain;
        _appliedProfile = _desiredProfile;
        _appliedProfile.defaultMux = _jobMux;
        _appliedProfile.defaultGain = _jobGain;
        _configurationState = ConfigurationState::VERIFIED;
        _clearHardwareConfigDirty();
        _configGeneration++;
        if (_configGeneration == 0) {
          _configGeneration = 1;
        }
        _conversionStarted = false;
        _conversionReady = true;
        _jobState = JobState::SINGLE_SHOT_READ_CONVERSION;
        continue;
      }

      case JobState::SINGLE_SHOT_READ_CONVERSION: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        uint16_t rawReg = 0;
        Status st = _readRegister16Tracked(cmd::REG_CONVERSION, rawReg);
        used++;
        if (!st.ok()) {
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _lastRawValue = static_cast<int16_t>(rawReg);
        _workingSample = SampleResult{};
        _workingSample.rawCode = _lastRawValue;
        st = rawToMicrovolts(_lastRawValue, _jobGain, _workingSample.microvolts);
        if (!st.ok()) {
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _workingSample.channelId = _channelRequest.channelId;
        _workingSample.mux = _jobMux;
        _workingSample.gain = _jobGain;
        _workingSample.dataRate = _desiredProfile.dataRate;
        _workingSample.flags = static_cast<uint16_t>(SampleFlag::CONFIG_VERIFIED);
        if (_lastRawValue == INT16_MAX) {
          _workingSample.flags |=
              static_cast<uint16_t>(SampleFlag::AT_POSITIVE_CODE_LIMIT);
        }
        if (_lastRawValue == INT16_MIN) {
          _workingSample.flags |=
              static_cast<uint16_t>(SampleFlag::AT_NEGATIVE_CODE_LIMIT);
        }
        _workingSample.configGeneration = _configGeneration;
        _sampleSequence++;
        if (_sampleSequence == 0) {
          _sampleSequence = 1;
        }
        _workingSample.sequence = _sampleSequence;
        _conversionReady = false;
        return _finishOperation(Status::Ok(), OperationState::SUCCEEDED,
                                used, true);
      }

      case JobState::WAIT_IDLE_AFTER_ABANDON: {
        const uint32_t conversionMs =
            (worstCaseConversionTimeUs(_desiredProfile.dataRate) + 999UL) / 1000UL;
        if (!elapsedAtLeast(_conversionStartMs, conversionMs, nowMs)) {
          return _pollResult(_abandonStatus, used, false);
        }
        _conversionStarted = false;
        _conversionReady = false;
        const OperationState terminalState =
            _abandonStatus.code == Err::CANCELLED
                ? OperationState::CANCELLED
                : (_abandonStatus.code == Err::TIMEOUT
                       ? OperationState::TIMED_OUT
                       : OperationState::FAILED);
        return _finishOperation(_abandonStatus, terminalState, used);
      }

      case JobState::SHUTDOWN_WRITE_CONFIG: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _writeRegister16Tracked(cmd::REG_CONFIG, _jobConfigRegister);
        used++;
        if (!st.ok()) {
          if (isUncertainWriteFailure(st.code)) {
            _configurationState = ConfigurationState::UNKNOWN;
            _markHardwareConfigDirtyIfClean(st);
          }
          return _finishOperation(st, OperationState::FAILED, used);
        }
        _jobAnyWriteConfirmed = true;
        _jobState = JobState::APPLY_VERIFY_CONFIG;
        continue;
      }

      default:
        return _finishOperation(
            Status::Error(Err::INDETERMINATE, "Invalid operation state"),
            OperationState::INDETERMINATE, used);
    }
  }
}

CancelDisposition ADS1115::cancelActiveOperation() {
  if (!_jobActive) {
    return CancelDisposition::NO_ACTIVE_OPERATION;
  }
  const Status cancelled = Status::Error(Err::CANCELLED, "Operation cancelled");
  if (_operationState == OperationState::RECONCILING &&
      _jobState == JobState::WAIT_IDLE_AFTER_ABANDON) {
    return CancelDisposition::RECONCILIATION_REQUIRED;
  }
  if (_operationKind == OperationKind::READ_SINGLE_SHOT &&
      _jobStartWriteAttempted &&
      _jobState != JobState::SINGLE_SHOT_READ_CONVERSION) {
    _abandonStatus = cancelled;
    _operationState = OperationState::RECONCILING;
    _jobState = JobState::WAIT_IDLE_AFTER_ABANDON;
    _configurationState = ConfigurationState::UNKNOWN;
    _markHardwareConfigDirty(cancelled);
    return CancelDisposition::RECONCILIATION_REQUIRED;
  }
  if (_operationKind == OperationKind::READ_SINGLE_SHOT &&
      _jobState == JobState::SINGLE_SHOT_READ_CONVERSION) {
    (void)_finishOperation(cancelled, OperationState::CANCELLED, 0);
    return CancelDisposition::CANCELLED_AFTER_EFFECT;
  }
  const bool effectPossible = _jobAnyWriteConfirmed || _jobStartWriteAttempted;
  if (effectPossible) {
    _configurationState = ConfigurationState::UNKNOWN;
    _markHardwareConfigDirty(cancelled);
  } else {
    _configurationState = _configurationStateBeforeOperation;
  }
  (void)_finishOperation(cancelled, OperationState::CANCELLED, 0);
  return effectPossible ? CancelDisposition::CANCELLED_AFTER_EFFECT
                        : CancelDisposition::CANCELLED_BEFORE_EFFECT;
}

Status ADS1115::takeResult(OperationToken token, OperationResult& out) {
  if (!_terminalResultAvailable) {
    return Status::Error(Err::RESULT_NOT_AVAILABLE, "No terminal result available");
  }
  if (!token.valid() || token.value != _terminalResult.token.value) {
    return Status::Error(Err::TOKEN_MISMATCH, "Operation token mismatch");
  }
  out = _terminalResult;
  _terminalResultAvailable = false;
  _terminalResult = OperationResult{};
  _operationKind = OperationKind::NONE;
  _operationState = OperationState::IDLE;
  _operationToken = OperationToken{};
  _jobState = JobState::IDLE;
  return Status::Ok();
}

Status ADS1115::getAppliedProfile(AppliedProfileSnapshot& out) const {
  out.profile = _appliedProfile;
  out.state = _configurationState;
  out.generation = _configGeneration;
  return _bound ? Status::Ok()
                : Status::Error(Err::NOT_INITIALIZED, "Driver not bound");
}

Status ADS1115::begin(const Config& config) {
  const Config requestedConfig = config;
  const bool priorConfigDirty = _hardwareConfigDirty;
  const Status priorConfigDirtyError = _hardwareConfigDirtyError;
  const uint8_t priorConfigDirtyAddress = _hardwareConfigDirtyAddress;

  if (requestedConfig.i2cWrite == nullptr || requestedConfig.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (requestedConfig.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "Timeout must be > 0");
  }
  if (requestedConfig.i2cAddress < kMinAddress || requestedConfig.i2cAddress > kMaxAddress) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address");
  }
  if (!isValidMux(requestedConfig.mux) || !isValidGain(requestedConfig.gain) ||
      !isValidDataRate(requestedConfig.dataRate) || !isValidMode(requestedConfig.mode) ||
      !isValidCompMode(requestedConfig.compMode) ||
      !isValidCompPolarity(requestedConfig.compPolarity) ||
      !isValidCompLatch(requestedConfig.compLatch) ||
      !isValidCompQueue(requestedConfig.compQueue)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid config enum value");
  }
  if (requestedConfig.alertRdyPin < -1) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid ALERT/RDY pin");
  }
  if (requestedConfig.alertRdyPin >= 0 && requestedConfig.gpioRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "ALERT/RDY gpioRead required");
  }

  DeviceProfile profile;
  profile.i2cAddress = requestedConfig.i2cAddress;
  profile.defaultMux = requestedConfig.mux;
  profile.defaultGain = requestedConfig.gain;
  profile.dataRate = requestedConfig.dataRate;
  profile.mode = requestedConfig.mode;
  profile.comparator.mode = requestedConfig.compMode;
  profile.comparator.polarity = requestedConfig.compPolarity;
  profile.comparator.latch = requestedConfig.compLatch;
  profile.comparator.queue = requestedConfig.compQueue;
  profile.comparator.lowThreshold = requestedConfig.compThresholdLow;
  profile.comparator.highThreshold = requestedConfig.compThresholdHigh;
  if (isAlertRdyModeConfigured(requestedConfig)) {
    profile.comparator.use = ComparatorUse::CONVERSION_READY;
  } else if (requestedConfig.compQueue == ComparatorQueue::DISABLE) {
    profile.comparator.use = ComparatorUse::OFF;
  } else {
    profile.comparator.use = ComparatorUse::THRESHOLD;
  }
  Status st = validateDeviceProfile(profile);
  if (!st.ok()) {
    return st;
  }

  DriverConfig driverConfig;
  driverConfig.i2cWrite = requestedConfig.i2cWrite;
  driverConfig.i2cWriteRead = requestedConfig.i2cWriteRead;
  driverConfig.i2cUser = requestedConfig.i2cUser;
  driverConfig.transferTimeoutMs = requestedConfig.i2cTimeoutMs;
  st = bind(driverConfig, profile);
  if (!st.ok()) {
    return st;
  }
  if (priorConfigDirty) {
    _hardwareConfigDirty = true;
    _hardwareConfigDirtyError = priorConfigDirtyError;
    _hardwareConfigDirtyAddress = priorConfigDirtyAddress;
    if (priorConfigDirtyAddress == profile.i2cAddress) {
      _configurationState = ConfigurationState::UNKNOWN;
    }
  }
  _config.nowMs = requestedConfig.nowMs;
  _config.cooperativeYield = requestedConfig.cooperativeYield;
  _config.timeUser = requestedConfig.timeUser;
  _config.alertRdyPin = requestedConfig.alertRdyPin;
  _config.gpioRead = requestedConfig.gpioRead;
  _config.gpioUser = requestedConfig.gpioUser;
  _config.offlineThreshold = requestedConfig.offlineThreshold == 0
                                 ? 1
                                 : requestedConfig.offlineThreshold;
  _config.strictInitVerify = true;

  const uint32_t nowMs = requestedConfig.nowMs != nullptr
                             ? requestedConfig.nowMs(requestedConfig.timeUser)
                             : 0;
  const uint32_t initBudgetMs =
      requestedConfig.i2cTimeoutMs > (static_cast<uint32_t>(INT32_MAX) - 8U) / 7U
          ? static_cast<uint32_t>(INT32_MAX)
          : requestedConfig.i2cTimeoutMs * 7U + 8U;
  OperationToken token;
  st = startInitialize(nowMs, nowMs + initBudgetMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  for (uint8_t step = 0; step < 4 && _jobActive; ++step) {
    (void)poll(nowMs, MAX_JOB_INSTRUCTIONS);
  }
  if (_jobActive) {
    return Status::Error(Err::INDETERMINATE, "Initialization did not terminate");
  }
  OperationResult result;
  st = takeResult(token, result);
  if (!st.ok()) {
    return st;
  }
  return result.status;
}

void ADS1115::tick(uint32_t nowMs) {
  (void)service(nowMs);
}

Status ADS1115::service(uint32_t nowMs) {
  if (!_bound) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return poll(nowMs, 1).status;
  }

  if (_conversionStarted && !_conversionReady) {
    if (_config.nowMs == nullptr && _conversionStartMs == UINT32_MAX) {
      _conversionStartMs = nowMs;
      return Status::Ok();
    }
    if ((nowMs - _conversionStartMs) >= getConversionTimeMs()) {
      bool ready = false;
      return _readConversionReadyAt(nowMs, ready);
    }
  }

  return Status::Ok();
}

void ADS1115::end() {
  unbind();
}

Status ADS1115::shutdown() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  const uint32_t nowMs = _config.nowMs != nullptr ? _nowMs() : 0;
  const uint32_t budgetMs =
      _config.i2cTimeoutMs > (static_cast<uint32_t>(INT32_MAX) - 2U) / 2U
          ? static_cast<uint32_t>(INT32_MAX)
          : _config.i2cTimeoutMs * 2U + 2U;
  OperationToken token;
  Status st = startShutdown(nowMs, nowMs + budgetMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  (void)poll(nowMs, 2);
  OperationResult result;
  st = takeResult(token, result);
  return st.ok() ? result.status : st;
}

// ============================================================================
// Diagnostics
// ============================================================================

Status ADS1115::probe() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  return _probeRaw();
}

Status ADS1115::_probeRaw() {
  uint16_t configReg = 0;
  Status st = _readRegister16Raw(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
      return st;
    }
    if (isDefiniteAddressAbsence(st.code)) {
      return Status::Error(Err::DEVICE_NOT_FOUND, "ADS1115 address not acknowledged",
                           st.detail);
    }
    return st;
  }
  return Status::Ok();
}

Status ADS1115::recover() {
  if (!_bound) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  const uint32_t nowMs = _config.nowMs != nullptr ? _nowMs() : 0;
  const uint32_t budgetMs =
      _config.i2cTimeoutMs > (static_cast<uint32_t>(INT32_MAX) - 8U) / 7U
          ? static_cast<uint32_t>(INT32_MAX)
          : _config.i2cTimeoutMs * 7U + 8U;
  OperationToken token;
  Status st = startRecover(nowMs, nowMs + budgetMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }
  for (uint8_t step = 0; step < 4 && _jobActive; ++step) {
    (void)poll(nowMs, MAX_JOB_INSTRUCTIONS);
  }
  if (_jobActive) {
    return Status::Error(Err::INDETERMINATE, "Recovery did not terminate");
  }
  OperationResult result;
  st = takeResult(token, result);
  return st.ok() ? result.status : st;
}

Status ADS1115::getSettings(SettingsSnapshot& out) const {
  out.initialized = _initialized;
  out.bound = _bound;
  out.state = _driverState;
  out.configurationState = _configurationState;
  out.configGeneration = _configGeneration;
  out.operationKind = _operationKind;
  out.operationState = _operationState;
  out.operationToken = _operationToken;
  out.terminalResultAvailable = _terminalResultAvailable;
  out.i2cAddress = _config.i2cAddress;
  out.i2cTimeoutMs = _config.i2cTimeoutMs;
  out.offlineThreshold = _config.offlineThreshold;
  out.strictInitVerify = _config.strictInitVerify;
  out.hasNowMsHook = (_config.nowMs != nullptr);
  out.timebaseAvailable = out.hasNowMsHook;
  out.hasGpioReadHook = (_config.gpioRead != nullptr);
  out.hasCooperativeYieldHook = (_config.cooperativeYield != nullptr);
  out.hardwareConfigDirty = _hardwareConfigDirty;
  out.hardwareConfigDirtyError = _hardwareConfigDirtyError;
  out.hardwareConfigDirtyAddress = _hardwareConfigDirtyAddress;
  out.hardwareConfigUncertain = _hardwareConfigDirty;
  out.lastConfigApplyError = _hardwareConfigDirtyError;
  out.alertRdyPin = _config.alertRdyPin;
  out.alertRdyPinConfigured = isAlertRdyPinConfigured();
  out.conversionReadyModeEnabled = isConversionReadyModeEnabled();
  out.usesAlertRdyPin = usesAlertRdyPinForConversionReady();
  out.mux = _config.mux;
  out.gain = _config.gain;
  out.dataRate = _config.dataRate;
  out.mode = _config.mode;
  out.compMode = _config.compMode;
  out.compPolarity = _config.compPolarity;
  out.compLatch = _config.compLatch;
  out.compQueue = _config.compQueue;
  out.compThresholdHigh = _config.compThresholdHigh;
  out.compThresholdLow = _config.compThresholdLow;
  out.conversionStarted = _conversionStarted;
  out.conversionReady = _conversionReady;
  out.conversionStartMs = (_conversionStartMs == UINT32_MAX) ? 0 : _conversionStartMs;
  out.lastRawValue = _lastRawValue;
  return Status::Ok();
}

bool ADS1115::isAlertRdyPinConfigured() const {
  return _config.alertRdyPin >= 0 && _config.gpioRead != nullptr;
}

bool ADS1115::isConversionReadyModeEnabled() const {
  return isAlertRdyModeConfigured(_config);
}

bool ADS1115::usesAlertRdyPinForConversionReady() const {
  return useAlertRdyPin(_config);
}

// ============================================================================
// Conversion API
// ============================================================================

Status ADS1115::startConversion() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::UNSUPPORTED_OPERATION, "Continuous mode active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  uint16_t configReg = _buildConfigRegister() | cmd::OS_START;
  const uint32_t attemptMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _markHardwareConfigDirtyIfClean(st);
    if (isUncertainWriteFailure(st.code)) {
      _conversionStarted = true;
      _conversionReady = false;
      _conversionStartMs = attemptMs;
      _configurationState = ConfigurationState::UNKNOWN;
    }
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = attemptMs;
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status ADS1115::startConversion(Mux mux) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidMux(mux)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mux");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::UNSUPPORTED_OPERATION, "Continuous mode active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  Mux prevMux = _config.mux;
  _config.mux = mux;

  uint16_t configReg = _buildConfigRegister() | cmd::OS_START;
  const uint32_t attemptMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _config.mux = prevMux;
    _markHardwareConfigDirtyIfClean(st);
    if (isUncertainWriteFailure(st.code)) {
      _conversionStarted = true;
      _conversionReady = false;
      _conversionStartMs = attemptMs;
      _configurationState = ConfigurationState::UNKNOWN;
    }
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = attemptMs;
  if (mux != prevMux) {
    _configurationState = ConfigurationState::UNKNOWN;
  }
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

bool ADS1115::conversionReady() {
  bool ready = false;
  (void)readConversionReady(ready);
  return ready;
}

Status ADS1115::conversionReady(bool& ready) {
  return readConversionReady(ready);
}

Status ADS1115::readConversionReady(bool& ready) {
  ready = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  const uint32_t nowMs = (_config.nowMs != nullptr) ? _nowMs() : _conversionStartMs;
  return _readConversionReadyAt(nowMs, ready);
}

Status ADS1115::_readConversionReadyAt(uint32_t nowMs, bool& ready) {
  ready = false;
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_conversionReady) {
    ready = true;
    return Status::Ok();
  }
  if (!_conversionStarted) {
    if (_config.mode == Mode::CONTINUOUS) {
      _conversionStarted = true;
      _conversionStartMs = nowMs;
    }
    return Status::Ok();
  }

  const uint32_t requiredPeriods =
      _config.mode == Mode::CONTINUOUS ? _continuousSettlePeriods : 1U;
  if ((nowMs - _conversionStartMs) < getConversionTimeMs() * requiredPeriods) {
    return Status::Ok();
  }

  if (useAlertRdyPin(_config)) {
    if (isAlertRdyAsserted(_config)) {
      _conversionStarted = (_config.mode == Mode::CONTINUOUS);
      _conversionReady = true;
      ready = true;
    }
    return Status::Ok();
  }

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionReady = true;
    _continuousSettlePeriods = 1;
    ready = true;
    return Status::Ok();
  }

  uint16_t configReg = 0;
  Status st = readRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return st;
  }

  if ((configReg & cmd::MASK_OS) == cmd::OS_IDLE) {
    if ((configReg & kConfigReadbackMask) !=
        (_buildConfigRegister() & kConfigReadbackMask)) {
      _configurationState = ConfigurationState::UNKNOWN;
      Status mismatch = Status::Error(Err::READBACK_MISMATCH,
                                      "Config changed during conversion", configReg);
      _markHardwareConfigDirty(mismatch);
      return mismatch;
    }
    _conversionStarted = false;
    _conversionReady = true;
    if (!_hardwareConfigDirty) {
      _configurationState = ConfigurationState::VERIFIED;
      _appliedProfile = _profileFromConfig();
      _configGeneration++;
      if (_configGeneration == 0) {
        _configGeneration = 1;
      }
    }
    ready = true;
  }

  return Status::Ok();
}

Status ADS1115::readRaw(int16_t& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }

  if (_config.mode == Mode::SINGLE_SHOT) {
    if (!_conversionReady) {
      bool ready = false;
      Status st = readConversionReady(ready);
      if (!st.ok()) {
        return st;
      }
      if (!ready) {
        return Status::Error(Err::CONVERSION_NOT_READY, "Conversion not ready");
      }
    }
  }

  return readLatestRaw(out);
}

Status ADS1115::readLatestRaw(int16_t& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }

  uint16_t rawReg = 0;
  Status st = readRegister16(cmd::REG_CONVERSION, rawReg);
  if (!st.ok()) {
    return st;
  }

  out = static_cast<int16_t>(rawReg);
  _lastRawValue = out;

  if (_config.mode == Mode::SINGLE_SHOT) {
    _conversionReady = false;
  } else {
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
    _continuousSettlePeriods = 1;
  }

  return Status::Ok();
}

Status ADS1115::readVoltage(float& volts) {
  if (_configurationState != ConfigurationState::VERIFIED ||
      _hardwareConfigDirty || _config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::CONFIG_UNKNOWN,
                         "Verified single-shot configuration required for scaled read");
  }
  int16_t raw = 0;
  Status st = readRaw(raw);
  if (!st.ok()) {
    return st;
  }
  volts = rawToVoltage(raw);
  return Status::Ok();
}

Status ADS1115::readBlocking(int16_t& out, uint32_t timeoutMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.nowMs == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "nowMs required for blocking reads");
  }
  if (timeoutMs == 0 || timeoutMs > static_cast<uint32_t>(INT32_MAX)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid blocking timeout");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_terminalResultAvailable) {
    return Status::Error(Err::BUSY, "Take terminal result before blocking read");
  }
  if (_config.mode != Mode::SINGLE_SHOT) {
    return Status::Error(Err::UNSUPPORTED_OPERATION,
                         "Blocking fresh reads require single-shot mode");
  }
  const uint32_t nowMs = _nowMs();
  const uint32_t deadlineMs = nowMs + timeoutMs;
  ChannelRequest request;
  request.mux = _config.mux;
  request.gain = _config.gain;
  OperationToken token;
  Status st = startRead(request, nowMs, deadlineMs, token);
  if (st.code != Err::IN_PROGRESS) {
    return st;
  }

  uint32_t lastObservedMs = _nowMs();
  uint32_t sameTickPolls = 0;

  while (_jobActive) {
    uint32_t loopNowMs = _nowMs();
    if (loopNowMs == lastObservedMs) {
      if (sameTickPolls >= kMaxSameTickPolls) {
        const Status stalled = Status::Error(Err::CLOCK_STALLED,
                                             "Timebase did not advance",
                                             static_cast<int32_t>(sameTickPolls));
        if (_jobStartWriteAttempted) {
          _abandonStatus = stalled;
          _operationState = OperationState::RECONCILING;
          _jobState = JobState::WAIT_IDLE_AFTER_ABANDON;
          _configurationState = ConfigurationState::UNKNOWN;
        } else {
          (void)_finishOperation(stalled, OperationState::FAILED, 0);
        }
        return stalled;
      }
      sameTickPolls++;
    } else {
      lastObservedMs = loopNowMs;
      sameTickPolls = 1;
    }

    PollResult progress = poll(loopNowMs, 1);
    if (progress.done) {
      OperationResult result;
      st = takeResult(token, result);
      if (!st.ok()) {
        return st;
      }
      if (!result.status.ok()) {
        return result.status;
      }
      if (!result.sampleValid) {
        return Status::Error(Err::RESULT_NOT_AVAILABLE, "Sample missing");
      }
      out = result.sample.rawCode;
      return Status::Ok();
    }
    if (progress.status.code == Err::TIMEOUT) {
      return progress.status;
    }
    _cooperativeYield();
  }
  return Status::Error(Err::INDETERMINATE, "Blocking read ended without result");
}

Status ADS1115::readBlockingVoltage(float& volts, uint32_t timeoutMs) {
  if (_configurationState != ConfigurationState::VERIFIED ||
      _hardwareConfigDirty || _config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::CONFIG_UNKNOWN,
                         "Verified single-shot configuration required for scaled read");
  }
  int16_t raw = 0;
  Status st = readBlocking(raw, timeoutMs);
  if (!st.ok()) {
    return st;
  }
  volts = rawToVoltage(raw);
  return Status::Ok();
}

Status ADS1115::startSingleShot() {
  return startSingleShot(_config.mux);
}

Status ADS1115::startSingleShot(Mux mux) {
  if (_config.nowMs == nullptr) {
    return Status::Error(Err::INVALID_CONFIG,
                         "nowMs required by compatibility staged API");
  }
  ChannelRequest request;
  request.mux = mux;
  request.gain = _config.gain;
  const uint32_t nowMs = _nowMs();
  const uint32_t transferMarginMs =
      _config.i2cTimeoutMs > (static_cast<uint32_t>(INT32_MAX) - 4U) / 3U
          ? static_cast<uint32_t>(INT32_MAX)
          : _config.i2cTimeoutMs * 3U + 4U;
  const uint32_t durationMs = operationDeadlineMs(
      1, _config.dataRate, transferMarginMs);
  OperationToken token;
  return startRead(request, nowMs, nowMs + durationMs, token);
}

PollResult ADS1115::pollSingleShot(uint32_t nowMs, uint8_t maxInstructions) {
  if (_operationKind != OperationKind::READ_SINGLE_SHOT &&
      (_jobActive || _terminalResultAvailable)) {
    return _pollResult(Status::Error(Err::BUSY, "Different operation active"), 0,
                       false);
  }
  return poll(nowMs, maxInstructions);
}

Status ADS1115::startApplyConfigJob() {
  if (_config.nowMs == nullptr) {
    return Status::Error(Err::INVALID_CONFIG,
                         "nowMs required by compatibility staged API");
  }
  const uint32_t nowMs = _nowMs();
  const uint32_t durationMs =
      _config.i2cTimeoutMs > (static_cast<uint32_t>(INT32_MAX) - 8U) / 6U
          ? static_cast<uint32_t>(INT32_MAX)
          : _config.i2cTimeoutMs * 6U + 8U;
  OperationToken token;
  return startApplyProfile(_profileFromConfig(), nowMs, nowMs + durationMs, token);
}

PollResult ADS1115::pollApplyConfig(uint32_t nowMs, uint8_t maxInstructions) {
  if (_operationKind != OperationKind::APPLY_PROFILE &&
      (_jobActive || _terminalResultAvailable)) {
    return _pollResult(Status::Error(Err::BUSY, "Different operation active"), 0,
                       false);
  }
  return poll(nowMs, maxInstructions);
}

void ADS1115::cancelJob() {
  (void)cancelActiveOperation();
}

// ============================================================================
// Configuration
// ============================================================================

Status ADS1115::setMux(Mux mux) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidMux(mux)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mux");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const Mux oldMux = _config.mux;
  _config.mux = mux;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.mux = oldMux;
  }
  return st;
}

Status ADS1115::setGain(Gain gain) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidGain(gain)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid gain");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const Gain oldGain = _config.gain;
  _config.gain = gain;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.gain = oldGain;
  }
  return st;
}

Status ADS1115::setDataRate(DataRate rate) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidDataRate(rate)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid data rate");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const DataRate oldDataRate = _config.dataRate;
  _config.dataRate = rate;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.dataRate = oldDataRate;
  }
  return st;
}

Status ADS1115::setMode(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mode");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_config.mode == Mode::SINGLE_SHOT && _singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const Mode oldMode = _config.mode;
  const bool oldConversionStarted = _conversionStarted;
  const bool oldConversionReady = _conversionReady;
  const uint32_t oldConversionStartMs = _conversionStartMs;
  _config.mode = mode;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.mode = oldMode;
    _conversionStarted = oldConversionStarted;
    _conversionReady = oldConversionReady;
    _conversionStartMs = oldConversionStartMs;
  }
  return st;
}

Status ADS1115::readConfig(uint16_t& config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  return readRegister16(cmd::REG_CONFIG, config);
}

Status ADS1115::writeConfig(uint16_t config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConfigValue(config)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid config value");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_config.mode == Mode::SINGLE_SHOT && _singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, config);
  if (!st.ok()) {
    _markHardwareConfigDirtyIfClean(st);
    return st;
  }

  _config.mux = static_cast<Mux>((config & cmd::MASK_MUX) >> cmd::BIT_MUX);
  _config.gain = decodePgaBits(static_cast<uint8_t>((config & cmd::MASK_PGA) >> cmd::BIT_PGA));
  _config.mode = static_cast<Mode>((config & cmd::MASK_MODE) >> cmd::BIT_MODE);
  _config.dataRate = static_cast<DataRate>((config & cmd::MASK_DR) >> cmd::BIT_DR);
  _config.compMode = static_cast<ComparatorMode>((config & cmd::MASK_COMP_MODE) >> cmd::BIT_COMP_MODE);
  _config.compPolarity = static_cast<ComparatorPolarity>((config & cmd::MASK_COMP_POL) >> cmd::BIT_COMP_POL);
  _config.compLatch = static_cast<ComparatorLatch>((config & cmd::MASK_COMP_LAT) >> cmd::BIT_COMP_LAT);
  _config.compQueue = static_cast<ComparatorQueue>((config & cmd::MASK_COMP_QUE) >> cmd::BIT_COMP_QUE);

  if (_config.mode == Mode::SINGLE_SHOT && ((config & cmd::MASK_OS) == cmd::OS_START)) {
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
  } else if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
    _continuousSettlePeriods = 2;
  } else {
    _conversionStarted = false;
    _conversionReady = false;
  }

  _configurationState = ConfigurationState::UNKNOWN;

  return Status::Ok();
}

// ============================================================================
// Comparator
// ============================================================================

Status ADS1115::setThresholds(int16_t low, int16_t high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  if (high <= low) {
    return Status::Error(Err::INVALID_PARAM, "High threshold must exceed low threshold");
  }

  Status st = _writeRegister16Tracked(cmd::REG_LO_THRESH, static_cast<uint16_t>(low));
  if (!st.ok()) {
    _markHardwareConfigDirtyIfClean(st);
    return st;
  }
  st = _writeRegister16Tracked(cmd::REG_HI_THRESH, static_cast<uint16_t>(high));
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  _config.compThresholdLow = low;
  _config.compThresholdHigh = high;
  _configurationState = ConfigurationState::UNKNOWN;
  return Status::Ok();
}

Status ADS1115::getThresholds(int16_t& low, int16_t& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }

  uint16_t lowReg = 0;
  uint16_t highReg = 0;
  Status st = readRegister16(cmd::REG_LO_THRESH, lowReg);
  if (!st.ok()) {
    return st;
  }
  st = readRegister16(cmd::REG_HI_THRESH, highReg);
  if (!st.ok()) {
    return st;
  }

  low = static_cast<int16_t>(lowReg);
  high = static_cast<int16_t>(highReg);
  _config.compThresholdLow = low;
  _config.compThresholdHigh = high;
  return Status::Ok();
}

Status ADS1115::setComparatorMode(ComparatorMode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator mode");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const ComparatorMode oldMode = _config.compMode;
  _config.compMode = mode;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.compMode = oldMode;
  }
  return st;
}

Status ADS1115::setComparatorPolarity(ComparatorPolarity polarity) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompPolarity(polarity)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator polarity");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const ComparatorPolarity oldPolarity = _config.compPolarity;
  _config.compPolarity = polarity;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.compPolarity = oldPolarity;
  }
  return st;
}

Status ADS1115::setComparatorLatch(ComparatorLatch latch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompLatch(latch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator latch");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const ComparatorLatch oldLatch = _config.compLatch;
  _config.compLatch = latch;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.compLatch = oldLatch;
  }
  return st;
}

Status ADS1115::setComparatorQueue(ComparatorQueue queue) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompQueue(queue)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator queue");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  const ComparatorQueue oldQueue = _config.compQueue;
  _config.compQueue = queue;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.compQueue = oldQueue;
  }
  return st;
}

Status ADS1115::enableConversionReadyPin() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }

  const int16_t oldLow = _config.compThresholdLow;
  const int16_t oldHigh = _config.compThresholdHigh;
  const ComparatorQueue oldQueue = _config.compQueue;
  const ComparatorMode oldMode = _config.compMode;
  const ComparatorLatch oldLatch = _config.compLatch;

  _config.compThresholdLow = 0;
  _config.compThresholdHigh = -32768;  // 0x8000 as int16_t
  _config.compQueue = ComparatorQueue::ASSERT_1;
  _config.compMode = ComparatorMode::TRADITIONAL;
  _config.compLatch = ComparatorLatch::NON_LATCHING;

  Status st = _applyConfig();
  if (!st.ok()) {
    _config.compThresholdLow = oldLow;
    _config.compThresholdHigh = oldHigh;
    _config.compQueue = oldQueue;
    _config.compMode = oldMode;
    _config.compLatch = oldLatch;
  }
  return st;
}

Status ADS1115::disableComparator() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }

  const ComparatorQueue oldQueue = _config.compQueue;
  _config.compQueue = ComparatorQueue::DISABLE;
  Status st = _writeConfigOnly();
  if (!st.ok()) {
    _config.compQueue = oldQueue;
  }
  return st;
}

// ============================================================================
// Utility
// ============================================================================

float ADS1115::rawToVoltage(int16_t raw) const {
  static constexpr float lsbTable[] = {
    187.5e-6f,   // FSR_6_144V
    125.0e-6f,   // FSR_4_096V
    62.5e-6f,    // FSR_2_048V
    31.25e-6f,   // FSR_1_024V
    15.625e-6f,  // FSR_0_512V
    7.8125e-6f   // FSR_0_256V
  };

  uint8_t index = static_cast<uint8_t>(_config.gain);
  if (index >= (sizeof(lsbTable) / sizeof(lsbTable[0]))) {
    index = static_cast<uint8_t>(Gain::FSR_2_048V);
  }
  return raw * lsbTable[index];
}

float ADS1115::getLsbVoltage() const {
  static constexpr float lsbTable[] = {
    187.5e-6f,
    125.0e-6f,
    62.5e-6f,
    31.25e-6f,
    15.625e-6f,
    7.8125e-6f
  };

  uint8_t index = static_cast<uint8_t>(_config.gain);
  if (index >= (sizeof(lsbTable) / sizeof(lsbTable[0]))) {
    index = static_cast<uint8_t>(Gain::FSR_2_048V);
  }
  return lsbTable[index];
}

uint32_t ADS1115::getConversionTimeMs() const {
  const uint32_t conversionUs = worstCaseConversionTimeUs(_config.dataRate);
  return conversionUs == 0 ? 0 : (conversionUs + 999UL) / 1000UL;
}

Status ADS1115::_jobBusyStatus() const {
  return Status::Error(Err::BUSY, "Poll job active");
}

uint8_t ADS1115::_instructionBudget(uint8_t maxInstructions) const {
  return (maxInstructions > MAX_JOB_INSTRUCTIONS) ? MAX_JOB_INSTRUCTIONS : maxInstructions;
}

PollResult ADS1115::_pollResult(Status status, uint8_t instructionsUsed, bool done) const {
  PollResult result;
  result.status = status;
  result.instructionsUsed = instructionsUsed;
  result.done = done;
  result.state = _jobState;
  result.token = _operationToken;
  result.kind = _operationKind;
  result.operationState = _operationState;
  return result;
}

PollResult ADS1115::_finishOperation(const Status& status, OperationState state,
                                     uint8_t transactionsUsed, bool sampleValid) {
  _jobActive = false;
  _operationState = state;
  _lastJobStatus = status;
  if (state == OperationState::SUCCEEDED) {
    _jobState = JobState::COMPLETE;
  } else if (state == OperationState::CANCELLED) {
    _jobState = JobState::CANCELLED;
  } else if (state == OperationState::TIMED_OUT) {
    _jobState = JobState::TIMED_OUT;
  } else {
    _jobState = JobState::FAILED;
  }
  _terminalResult = OperationResult{};
  _terminalResult.token = _operationToken;
  _terminalResult.kind = _operationKind;
  _terminalResult.state = state;
  _terminalResult.status = status;
  _terminalResult.sampleValid = sampleValid;
  _terminalResult.hardwareStateUncertain =
      _configurationState == ConfigurationState::UNKNOWN || _hardwareConfigDirty;
  if (sampleValid) {
    _terminalResult.sample = _workingSample;
  }
  _terminalResultAvailable = true;
  return _pollResult(status, transactionsUsed, true);
}

bool ADS1115::_deadlineReached(uint32_t nowMs) const {
  return static_cast<int32_t>(nowMs - _operationDeadlineMs) >= 0;
}

bool ADS1115::_singleShotMayBeActive() const {
  return (_config.mode == Mode::SINGLE_SHOT && _conversionStarted) ||
         (_operationKind == OperationKind::READ_SINGLE_SHOT &&
          (_jobStartWriteAttempted || _jobActive));
}

Status ADS1115::_activeHardwareBusyStatus() const {
  return Status::Error(Err::BUSY, "Conversion may still be active");
}

void ADS1115::_resetOperationScratch() {
  _jobActive = false;
  _jobState = JobState::IDLE;
  _lastJobStatus = Status::Ok();
  _jobConfigRegister = 0;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMux = Mux::AIN0_GND;
  _jobGain = Gain::FSR_2_048V;
  _channelRequest = ChannelRequest{};
  _jobStartWriteAttempted = false;
  _jobAnyWriteConfirmed = false;
  _jobNextReadyPollMs = 0;
  _abandonStatus = Status::Ok();
  _workingSample = SampleResult{};
}

void ADS1115::_loadProfileIntoConfig(const DeviceProfile& profile) {
  _config.i2cAddress = profile.i2cAddress;
  _config.mux = profile.defaultMux;
  _config.gain = profile.defaultGain;
  _config.dataRate = profile.dataRate;
  _config.mode = profile.mode;
  _config.compMode = profile.comparator.mode;
  _config.compPolarity = profile.comparator.polarity;
  _config.compLatch = profile.comparator.latch;
  _config.compQueue = profile.comparator.queue;
  _config.compThresholdLow = profile.comparator.lowThreshold;
  _config.compThresholdHigh = profile.comparator.highThreshold;
}

DeviceProfile ADS1115::_profileFromConfig() const {
  DeviceProfile profile;
  profile.i2cAddress = _config.i2cAddress;
  profile.defaultMux = _config.mux;
  profile.defaultGain = _config.gain;
  profile.dataRate = _config.dataRate;
  profile.mode = _config.mode;
  profile.comparator.mode = _config.compMode;
  profile.comparator.polarity = _config.compPolarity;
  profile.comparator.latch = _config.compLatch;
  profile.comparator.queue = _config.compQueue;
  profile.comparator.lowThreshold = _config.compThresholdLow;
  profile.comparator.highThreshold = _config.compThresholdHigh;
  if (isAlertRdyModeConfigured(_config)) {
    profile.comparator.use = ComparatorUse::CONVERSION_READY;
  } else if (_config.compQueue == ComparatorQueue::DISABLE) {
    profile.comparator.use = ComparatorUse::OFF;
  } else {
    profile.comparator.use = ComparatorUse::THRESHOLD;
  }
  return profile;
}

uint16_t ADS1115::_buildConfigRegisterFor(const DeviceProfile& profile, Mux mux,
                                           Gain gain) const {
  uint16_t config = 0;
  config |= (static_cast<uint16_t>(mux) << cmd::BIT_MUX) & cmd::MASK_MUX;
  config |= (static_cast<uint16_t>(gain) << cmd::BIT_PGA) & cmd::MASK_PGA;
  config |= (static_cast<uint16_t>(profile.mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  config |= (static_cast<uint16_t>(profile.dataRate) << cmd::BIT_DR) & cmd::MASK_DR;
  config |= (static_cast<uint16_t>(profile.comparator.mode) << cmd::BIT_COMP_MODE) &
            cmd::MASK_COMP_MODE;
  config |= (static_cast<uint16_t>(profile.comparator.polarity) << cmd::BIT_COMP_POL) &
            cmd::MASK_COMP_POL;
  config |= (static_cast<uint16_t>(profile.comparator.latch) << cmd::BIT_COMP_LAT) &
            cmd::MASK_COMP_LAT;
  config |= (static_cast<uint16_t>(profile.comparator.queue) << cmd::BIT_COMP_QUE) &
            cmd::MASK_COMP_QUE;
  return config;
}

// ============================================================================
// Transport Wrappers
// ============================================================================

Status ADS1115::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                 uint8_t* rxBuf, size_t rxLen) {
  if (_config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback missing");
  }
  if (txBuf == nullptr || txLen == 0 || rxBuf == nullptr || rxLen == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C read parameters");
  }
  const uint32_t timeoutMs = (_jobActive && _activeTransferTimeoutMs != 0)
                                 ? _activeTransferTimeoutMs
                                 : _config.i2cTimeoutMs;
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen,
                              rxBuf, rxLen, timeoutMs,
                              _config.i2cUser);
}

Status ADS1115::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  if (_config.i2cWrite == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback missing");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }
  const uint32_t timeoutMs = (_jobActive && _activeTransferTimeoutMs != 0)
                                 ? _activeTransferTimeoutMs
                                 : _config.i2cTimeoutMs;
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          timeoutMs, _config.i2cUser);
}

Status ADS1115::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen) {
  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status ADS1115::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  Status st = _i2cWriteRaw(buf, len);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

// ============================================================================
// Register Access
// ============================================================================

Status ADS1115::readRegister16(uint8_t reg, uint16_t& value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  return _readRegister16Tracked(reg, value);
}

Status ADS1115::writeRegister16(uint8_t reg, uint16_t value) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register");
  }
  if (!isWritableRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Register is read-only");
  }
  if (_jobActive) {
    return _jobBusyStatus();
  }
  if (_singleShotMayBeActive()) {
    return _activeHardwareBusyStatus();
  }
  Status st = _writeRegister16Tracked(reg, value);
  if (!st.ok()) {
    if (st.code != Err::OFFLINE) {
      _markHardwareConfigDirty(st);
    }
    return st;
  }
  _markHardwareConfigDirty(
      Status::Error(Err::HARDWARE_CONFIG_DIRTY,
                    "Raw register write changed hardware config", reg));
  return Status::Ok();
}

Status ADS1115::_readRegister16Tracked(uint8_t reg, uint16_t& value) {
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register");
  }
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadTracked(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
  return Status::Ok();
}

Status ADS1115::_writeRegister16Tracked(uint8_t reg, uint16_t value) {
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register");
  }
  if (!isWritableRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Register is read-only");
  }
  uint8_t tx[3] = {
    reg,
    static_cast<uint8_t>((value >> 8) & 0xFF),
    static_cast<uint8_t>(value & 0xFF)
  };
  return _i2cWriteTracked(tx, sizeof(tx));
}

Status ADS1115::_readRegister16Raw(uint8_t reg, uint16_t& value) {
  if (!isValidRegister(reg)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid register");
  }
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadRaw(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
  return Status::Ok();
}

// ============================================================================
// Health Tracking
// ============================================================================

Status ADS1115::_updateHealth(const Status& st) {
  if (!_initialized || st.inProgress()) {
    return st;
  }

  uint32_t nowMs = _nowMs();

  if (st.ok()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }

    _driverState = DriverState::READY;
  } else {
    _lastErrorMs = nowMs;
    _lastError = st;

    if (_consecutiveFailures < UINT8_MAX) {
      _consecutiveFailures++;
    }
    if (_totalFailures < UINT32_MAX) {
      _totalFailures++;
    }

    if (_consecutiveFailures >= _config.offlineThreshold) {
      _driverState = DriverState::OFFLINE;
    } else {
      _driverState = DriverState::DEGRADED;
    }
  }

  return st;
}

// ============================================================================
// Internal
// ============================================================================

Status ADS1115::_applyConfig() {
  Status st = _writeRegister16Tracked(cmd::REG_LO_THRESH,
                                      static_cast<uint16_t>(_config.compThresholdLow));
  if (!st.ok()) {
    _markHardwareConfigDirtyIfClean(st);
    return st;
  }
  st = _writeRegister16Tracked(cmd::REG_HI_THRESH,
                               static_cast<uint16_t>(_config.compThresholdHigh));
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }
  st = _writeRegister16Tracked(cmd::REG_CONFIG, _buildConfigRegister());
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  const bool requireReadback = _config.strictInitVerify || _hardwareConfigDirty;
  if (requireReadback) {
    st = _verifyConfigReadback();
    if (!st.ok()) {
      _markHardwareConfigDirty(st);
      return st;
    }
  }

  _clearHardwareConfigDirty();

  if (requireReadback) {
    _configurationState = ConfigurationState::VERIFIED;
    _appliedProfile = _profileFromConfig();
    _desiredProfile = _appliedProfile;
    _configGeneration++;
    if (_configGeneration == 0) {
      _configGeneration = 1;
    }
  } else {
    _configurationState = ConfigurationState::UNKNOWN;
  }

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
    _continuousSettlePeriods = 2;
  } else {
    _conversionStarted = false;
    _conversionStartMs = 0;
  }
  _conversionReady = false;
  return Status::Ok();
}

Status ADS1115::_writeConfigOnly() {
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, _buildConfigRegister());
  if (!st.ok()) {
    _markHardwareConfigDirtyIfClean(st);
    return st;
  }

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
    _continuousSettlePeriods = 2;
  } else {
    _conversionStarted = false;
    _conversionStartMs = 0;
  }
  _conversionReady = false;
  _configurationState = ConfigurationState::UNKNOWN;
  return Status::Ok();
}

Status ADS1115::_verifyConfigReadback() {
  uint16_t lowReg = 0;
  Status st = _readRegister16Tracked(cmd::REG_LO_THRESH, lowReg);
  if (!st.ok()) {
    return st;
  }
  if (lowReg != static_cast<uint16_t>(_config.compThresholdLow)) {
    return Status::Error(Err::READBACK_MISMATCH, "Low threshold readback mismatch",
                         static_cast<int32_t>(lowReg));
  }

  uint16_t highReg = 0;
  st = _readRegister16Tracked(cmd::REG_HI_THRESH, highReg);
  if (!st.ok()) {
    return st;
  }
  if (highReg != static_cast<uint16_t>(_config.compThresholdHigh)) {
    return Status::Error(Err::READBACK_MISMATCH, "High threshold readback mismatch",
                         static_cast<int32_t>(highReg));
  }

  uint16_t configReg = 0;
  st = _readRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return st;
  }
  const uint16_t expectedConfig = _buildConfigRegister() & kConfigReadbackMask;
  const uint16_t observedConfig = configReg & kConfigReadbackMask;
  if (observedConfig != expectedConfig) {
    return Status::Error(Err::READBACK_MISMATCH, "Config readback mismatch",
                         static_cast<int32_t>(configReg));
  }
  return Status::Ok();
}

Status ADS1115::_verifyJobReadback(uint8_t reg, uint16_t expected, const char* message) {
  uint16_t observed = 0;
  Status st = _readRegister16Tracked(reg, observed);
  if (!st.ok()) {
    return st;
  }

  uint16_t expectedComparable = expected;
  uint16_t observedComparable = observed;
  if (reg == cmd::REG_CONFIG) {
    expectedComparable &= kConfigReadbackMask;
    observedComparable &= kConfigReadbackMask;
  }
  if (observedComparable != expectedComparable) {
    return Status::Error(Err::READBACK_MISMATCH, message,
                         static_cast<int32_t>(observed));
  }
  return Status::Ok();
}

void ADS1115::_markHardwareConfigDirty(const Status& st) {
  _hardwareConfigDirty = true;
  _hardwareConfigDirtyError = st;
  _hardwareConfigDirtyAddress = _config.i2cAddress;
  if (_bound) {
    _configurationState = ConfigurationState::UNKNOWN;
  }
}

void ADS1115::_markHardwareConfigDirtyIfClean(const Status& st) {
  if (_hardwareConfigDirty || !isUncertainWriteFailure(st.code)) {
    return;
  }
  _markHardwareConfigDirty(st);
}

void ADS1115::_clearHardwareConfigDirty() {
  _hardwareConfigDirty = false;
  _hardwareConfigDirtyError = Status::Ok();
  _hardwareConfigDirtyAddress = kInvalidDirtyAddress;
}

uint16_t ADS1115::_buildConfigRegister() const {
  uint16_t config = 0;
  config |= (static_cast<uint16_t>(_config.mux) << cmd::BIT_MUX) & cmd::MASK_MUX;
  config |= (static_cast<uint16_t>(_config.gain) << cmd::BIT_PGA) & cmd::MASK_PGA;
  config |= (static_cast<uint16_t>(_config.mode) << cmd::BIT_MODE) & cmd::MASK_MODE;
  config |= (static_cast<uint16_t>(_config.dataRate) << cmd::BIT_DR) & cmd::MASK_DR;
  config |= (static_cast<uint16_t>(_config.compMode) << cmd::BIT_COMP_MODE) & cmd::MASK_COMP_MODE;
  config |= (static_cast<uint16_t>(_config.compPolarity) << cmd::BIT_COMP_POL) & cmd::MASK_COMP_POL;
  config |= (static_cast<uint16_t>(_config.compLatch) << cmd::BIT_COMP_LAT) & cmd::MASK_COMP_LAT;
  config |= (static_cast<uint16_t>(_config.compQueue) << cmd::BIT_COMP_QUE) & cmd::MASK_COMP_QUE;
  return config;
}

uint16_t ADS1115::_buildConfigRegisterForMux(Mux mux) const {
  uint16_t config = _buildConfigRegister();
  config &= static_cast<uint16_t>(~cmd::MASK_MUX);
  config |= (static_cast<uint16_t>(mux) << cmd::BIT_MUX) & cmd::MASK_MUX;
  return config;
}

uint32_t ADS1115::_nowMs() const {
  if (_config.nowMs != nullptr) {
    return _config.nowMs(_config.timeUser);
  }
  return 0;
}

void ADS1115::_cooperativeYield() const {
  if (_config.cooperativeYield != nullptr) {
    _config.cooperativeYield(_config.timeUser);
  }
}

} // namespace ADS1115
