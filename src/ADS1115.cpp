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

bool elapsedAtLeast(uint32_t startMs, uint32_t intervalMs, uint32_t nowMs) {
  return (nowMs - startMs) >= intervalMs;
}

class ScopedOfflineI2cAllowance {
public:
  explicit ScopedOfflineI2cAllowance(bool& flag, bool allow) : _flag(flag), _old(flag) {
    _flag = allow;
  }

  ~ScopedOfflineI2cAllowance() {
    _flag = _old;
  }

  ScopedOfflineI2cAllowance(const ScopedOfflineI2cAllowance&) = delete;
  ScopedOfflineI2cAllowance& operator=(const ScopedOfflineI2cAllowance&) = delete;

private:
  bool& _flag;
  bool _old;
};

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status ADS1115::begin(const Config& config) {
  const Config requestedConfig = config;

  _config = Config{};
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _allowOfflineI2c = false;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _lastRawValue = 0;
  _jobActive = false;
  _jobState = JobState::IDLE;
  _lastJobStatus = Status::Ok();
  _jobConfigRegister = 0;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMuxOverride = false;
  _jobNeedsReadback = false;
  _jobMux = Mux::AIN0_GND;

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

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

  _config = requestedConfig;
  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  auto failBeginAfterConfig = [this](const Status& failure) {
    _config = Config{};
    _allowOfflineI2c = false;
    _conversionStarted = false;
    _conversionReady = false;
    _conversionStartMs = 0;
    _lastRawValue = 0;
    _jobActive = false;
    _jobState = JobState::IDLE;
    _lastJobStatus = Status::Ok();
    _jobConfigRegister = 0;
    _jobThresholdLow = 0;
    _jobThresholdHigh = 0;
    _jobMuxOverride = false;
    _jobNeedsReadback = false;
    _jobMux = Mux::AIN0_GND;
    return failure;
  };

  Status st = probe();
  if (!st.ok()) {
    return failBeginAfterConfig(st);
  }

  st = _applyConfig();
  if (!st.ok()) {
    return failBeginAfterConfig(st);
  }

  _initialized = true;
  _driverState = DriverState::READY;
  return Status::Ok();
}

void ADS1115::tick(uint32_t nowMs) {
  (void)service(nowMs);
}

Status ADS1115::service(uint32_t nowMs) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
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
  if (_initialized && _driverState != DriverState::OFFLINE) {
    (void)shutdown();
  }

  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _lastRawValue = 0;
  _jobActive = false;
  _jobState = JobState::IDLE;
  _lastJobStatus = Status::Ok();
  _jobConfigRegister = 0;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMuxOverride = false;
  _jobNeedsReadback = false;
  _jobMux = Mux::AIN0_GND;
}

Status ADS1115::shutdown() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t configReg = _buildConfigRegister();
  configReg &= static_cast<uint16_t>(~cmd::MASK_MODE);
  configReg |= (static_cast<uint16_t>(Mode::SINGLE_SHOT) << cmd::BIT_MODE) & cmd::MASK_MODE;

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return st;
  }

  _config.mode = Mode::SINGLE_SHOT;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  return Status::Ok();
}

// ============================================================================
// Diagnostics
// ============================================================================

Status ADS1115::probe() {
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
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  const bool startedOffline = (_driverState == DriverState::OFFLINE);
  ScopedOfflineI2cAllowance allowOfflineI2c(_allowOfflineI2c, true);
  Status result = [&]() -> Status {
    uint16_t configReg = 0;
    Status st = readRegister16(cmd::REG_CONFIG, configReg);
    if (!st.ok()) {
      return st;
    }

    _conversionStarted = false;
    _conversionReady = false;
    _conversionStartMs = 0;

    st = _applyConfig();
    if (!st.ok()) {
      return st;
    }

    return Status::Ok();
  }();
  if (startedOffline && !result.ok() && !result.inProgress()) {
    _reassertOfflineLatch();
  }
  return result;
}

Status ADS1115::getSettings(SettingsSnapshot& out) const {
  out.initialized = _initialized;
  out.state = _driverState;
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
  if (_config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::UNSUPPORTED_OPERATION, "Continuous mode active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  uint16_t configReg = _buildConfigRegister() | cmd::OS_START;
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

Status ADS1115::startConversion(Mux mux) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidMux(mux)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mux");
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
  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _config.mux = prevMux;
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
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

  if ((nowMs - _conversionStartMs) < getConversionTimeMs()) {
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
    ready = true;
    return Status::Ok();
  }

  uint16_t configReg = 0;
  Status st = readRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return st;
  }

  if ((configReg & cmd::MASK_OS) == cmd::OS_IDLE) {
    _conversionStarted = false;
    _conversionReady = true;
    ready = true;
  }

  return Status::Ok();
}

Status ADS1115::readRaw(int16_t& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
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
  }

  return Status::Ok();
}

Status ADS1115::readVoltage(float& volts) {
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
  if (_config.mode == Mode::CONTINUOUS) {
    return readRaw(out);
  }

  Status st = startConversion();
  if (st.code != Err::IN_PROGRESS && st.code != Err::BUSY) {
    return st;
  }

  const uint32_t nowMs = _nowMs();
  const uint32_t convTimeMs = getConversionTimeMs();
  const uint32_t deadlineMs = nowMs + timeoutMs;

  // Calculate expected ready time, accounting for existing conversion
  uint32_t readyAtMs;
  if (st.code == Err::BUSY) {
    const uint32_t elapsed = nowMs - _conversionStartMs;
    readyAtMs = (elapsed >= convTimeMs) ? nowMs : (nowMs + convTimeMs - elapsed);
  } else {
    readyAtMs = nowMs + convTimeMs;
  }

  static constexpr uint32_t kMaxSameTickPolls = 65535U;
  uint32_t lastObservedMs = _nowMs();
  uint32_t sameTickPolls = 0;
  uint32_t lastReadyPollMs = 0;
  bool hasReadyPollMs = false;

  while (static_cast<int32_t>(_nowMs() - deadlineMs) < 0) {
    uint32_t loopNowMs = _nowMs();
    if (loopNowMs == lastObservedMs) {
      if (sameTickPolls >= kMaxSameTickPolls) {
        _conversionStarted = false;
        _conversionReady = false;
        return Status::Error(Err::TIMEOUT, "Conversion timeout");
      }
      sameTickPolls++;
    } else {
      lastObservedMs = loopNowMs;
      sameTickPolls = 1;
    }

    if (static_cast<int32_t>(loopNowMs - readyAtMs) < 0) {
      _cooperativeYield();  // Feed watchdog or cooperative scheduler.
      continue;
    }
    if (hasReadyPollMs && loopNowMs == lastReadyPollMs) {
      _cooperativeYield();
      continue;
    }
    hasReadyPollMs = true;
    lastReadyPollMs = loopNowMs;

    Status readSt = readRaw(out);
    if (readSt.ok()) {
      return Status::Ok();
    }
    if (readSt.code != Err::CONVERSION_NOT_READY) {
      return readSt;
    }
    _cooperativeYield();
  }

  // Timeout: clean up stale conversion state so startConversion() doesn't
  // permanently return BUSY on subsequent calls.
  _conversionStarted = false;
  _conversionReady = false;
  return Status::Error(Err::TIMEOUT, "Conversion timeout");
}

Status ADS1115::readBlockingVoltage(float& volts, uint32_t timeoutMs) {
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
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_isOffline()) {
    return _offlineStatus();
  }
  if (!isValidMux(mux)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mux");
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::UNSUPPORTED_OPERATION, "Continuous mode active");
  }
  if (_jobActive || _conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  _jobConfigRegister = _buildConfigRegisterForMux(mux) | cmd::OS_START;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMuxOverride = (mux != _config.mux);
  _jobNeedsReadback = false;
  _jobMux = mux;
  _jobActive = true;
  _jobState = JobState::SINGLE_SHOT_WRITE_CONFIG;
  _lastJobStatus = Status{Err::IN_PROGRESS, 0, "Single-shot job started"};
  return _lastJobStatus;
}

PollResult ADS1115::pollSingleShot(uint32_t nowMs, uint8_t maxInstructions) {
  if (!_initialized) {
    return _pollResult(Status::Error(Err::NOT_INITIALIZED, "Driver not initialized"),
                       0, true);
  }
  if (!_jobActive) {
    return _pollResult(_lastJobStatus, 0, true);
  }
  if (_isOffline()) {
    return _failJob(_offlineStatus(), 0);
  }
  if (_jobState != JobState::SINGLE_SHOT_WRITE_CONFIG &&
      _jobState != JobState::SINGLE_SHOT_WAIT_CONVERSION &&
      _jobState != JobState::SINGLE_SHOT_POLL_READY &&
      _jobState != JobState::SINGLE_SHOT_READ_CONVERSION) {
    return _pollResult(Status::Error(Err::BUSY, "Different job active"), 0, false);
  }

  const uint8_t budget = _instructionBudget(maxInstructions);
  uint8_t used = 0;

  while (true) {
    switch (_jobState) {
      case JobState::SINGLE_SHOT_WRITE_CONFIG: {
        if (used >= budget) {
          return _pollResult(_lastJobStatus, used, false);
        }
        Status st = _writeRegister16Tracked(cmd::REG_CONFIG, _jobConfigRegister);
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        if (_jobMuxOverride) {
          _config.mux = _jobMux;
        }
        _conversionStarted = true;
        _conversionReady = false;
        _conversionStartMs = nowMs;
        _jobState = JobState::SINGLE_SHOT_WAIT_CONVERSION;
        _lastJobStatus = Status{Err::IN_PROGRESS, 0, "Single-shot conversion started"};
        return _pollResult(_lastJobStatus, used, false);
      }

      case JobState::SINGLE_SHOT_WAIT_CONVERSION:
        if (useAlertRdyPin(_config)) {
          if (!isAlertRdyAsserted(_config)) {
            return _pollResult(_lastJobStatus, used, false);
          }
          _conversionStarted = false;
          _conversionReady = true;
          _jobState = JobState::SINGLE_SHOT_READ_CONVERSION;
          continue;
        }
        if (!elapsedAtLeast(_conversionStartMs, getConversionTimeMs(), nowMs)) {
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
          return _failJob(st, used);
        }
        if ((configReg & cmd::MASK_OS) != cmd::OS_IDLE) {
          return _pollResult(_lastJobStatus, used, false);
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
          return _failJob(st, used);
        }
        _lastRawValue = static_cast<int16_t>(rawReg);
        _conversionStarted = false;
        _conversionReady = false;
        return _finishJob(used);
      }

      default:
        return _pollResult(Status::Error(Err::BUSY, "Different job active"), used, false);
    }
  }
}

Status ADS1115::startApplyConfigJob() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_isOffline()) {
    return _offlineStatus();
  }
  if (_jobActive) {
    return Status::Error(Err::BUSY, "Job already active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  _jobThresholdLow = _config.compThresholdLow;
  _jobThresholdHigh = _config.compThresholdHigh;
  _jobConfigRegister = _buildConfigRegister();
  _jobMuxOverride = false;
  _jobNeedsReadback = _config.strictInitVerify || _hardwareConfigDirty;
  _jobMux = _config.mux;
  _jobActive = true;
  _jobState = JobState::APPLY_WRITE_LOW_THRESHOLD;
  _lastJobStatus = Status{Err::IN_PROGRESS, 0, "Apply config job started"};
  return _lastJobStatus;
}

PollResult ADS1115::pollApplyConfig(uint32_t nowMs, uint8_t maxInstructions) {
  if (!_initialized) {
    return _pollResult(Status::Error(Err::NOT_INITIALIZED, "Driver not initialized"),
                       0, true);
  }
  if (!_jobActive) {
    return _pollResult(_lastJobStatus, 0, true);
  }
  if (_isOffline()) {
    return _failJob(_offlineStatus(), 0);
  }
  if (_jobState != JobState::APPLY_WRITE_LOW_THRESHOLD &&
      _jobState != JobState::APPLY_WRITE_HIGH_THRESHOLD &&
      _jobState != JobState::APPLY_WRITE_CONFIG &&
      _jobState != JobState::APPLY_VERIFY_LOW_THRESHOLD &&
      _jobState != JobState::APPLY_VERIFY_HIGH_THRESHOLD &&
      _jobState != JobState::APPLY_VERIFY_CONFIG) {
    return _pollResult(Status::Error(Err::BUSY, "Different job active"), 0, false);
  }

  const uint8_t budget = _instructionBudget(maxInstructions);
  uint8_t used = 0;

  while (used < budget) {
    switch (_jobState) {
      case JobState::APPLY_WRITE_LOW_THRESHOLD: {
        Status st = _writeRegister16Tracked(cmd::REG_LO_THRESH,
                                            static_cast<uint16_t>(_jobThresholdLow));
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        _jobState = JobState::APPLY_WRITE_HIGH_THRESHOLD;
        break;
      }

      case JobState::APPLY_WRITE_HIGH_THRESHOLD: {
        Status st = _writeRegister16Tracked(cmd::REG_HI_THRESH,
                                            static_cast<uint16_t>(_jobThresholdHigh));
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        _jobState = JobState::APPLY_WRITE_CONFIG;
        break;
      }

      case JobState::APPLY_WRITE_CONFIG: {
        Status st = _writeRegister16Tracked(cmd::REG_CONFIG, _jobConfigRegister);
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        if (_jobNeedsReadback) {
          _jobState = JobState::APPLY_VERIFY_LOW_THRESHOLD;
          break;
        }
        if (_config.mode == Mode::CONTINUOUS) {
          _conversionStarted = true;
          _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : nowMs;
        } else {
          _conversionStarted = false;
          _conversionStartMs = 0;
        }
        _conversionReady = false;
        return _finishJob(used);
      }

      case JobState::APPLY_VERIFY_LOW_THRESHOLD: {
        Status st = _verifyJobReadback(cmd::REG_LO_THRESH,
                                       static_cast<uint16_t>(_jobThresholdLow),
                                       "Low threshold readback mismatch");
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        _jobState = JobState::APPLY_VERIFY_HIGH_THRESHOLD;
        break;
      }

      case JobState::APPLY_VERIFY_HIGH_THRESHOLD: {
        Status st = _verifyJobReadback(cmd::REG_HI_THRESH,
                                       static_cast<uint16_t>(_jobThresholdHigh),
                                       "High threshold readback mismatch");
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        _jobState = JobState::APPLY_VERIFY_CONFIG;
        break;
      }

      case JobState::APPLY_VERIFY_CONFIG: {
        Status st = _verifyJobReadback(cmd::REG_CONFIG, _jobConfigRegister,
                                       "Config readback mismatch");
        used++;
        if (!st.ok()) {
          return _failJob(st, used);
        }
        if (_config.mode == Mode::CONTINUOUS) {
          _conversionStarted = true;
          _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : nowMs;
        } else {
          _conversionStarted = false;
          _conversionStartMs = 0;
        }
        _conversionReady = false;
        return _finishJob(used);
      }

      default:
        return _pollResult(Status::Error(Err::BUSY, "Different job active"), used, false);
    }
  }

  return _pollResult(_lastJobStatus, used, false);
}

void ADS1115::cancelJob() {
  _jobActive = false;
  _jobState = JobState::IDLE;
  _lastJobStatus = Status::Ok();
  _jobConfigRegister = 0;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMuxOverride = false;
  _jobNeedsReadback = false;
  _jobMux = _config.mux;
  _conversionStarted = false;
  _conversionReady = false;
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
  return readRegister16(cmd::REG_CONFIG, config);
}

Status ADS1115::writeConfig(uint16_t config) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidConfigValue(config)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid config value");
  }

  Status st = _writeRegister16Tracked(cmd::REG_CONFIG, config);
  if (!st.ok()) {
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
  } else {
    _conversionStarted = false;
    _conversionReady = false;
  }

  return Status::Ok();
}

// ============================================================================
// Comparator
// ============================================================================

Status ADS1115::setThresholds(int16_t low, int16_t high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  Status st = _writeRegister16Tracked(cmd::REG_LO_THRESH, static_cast<uint16_t>(low));
  if (!st.ok()) {
    return st;
  }
  st = _writeRegister16Tracked(cmd::REG_HI_THRESH, static_cast<uint16_t>(high));
  if (!st.ok()) {
    _markHardwareConfigDirty(st);
    return st;
  }

  _config.compThresholdLow = low;
  _config.compThresholdHigh = high;
  return Status::Ok();
}

Status ADS1115::getThresholds(int16_t& low, int16_t& high) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
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
  static constexpr uint32_t timeTable[] = {
    125 + 5,  // 8 SPS
    63 + 5,   // 16 SPS
    32 + 5,   // 32 SPS
    16 + 5,   // 64 SPS
    8 + 2,    // 128 SPS
    4 + 2,    // 250 SPS
    3 + 1,    // 475 SPS
    2 + 1     // 860 SPS
  };

  uint8_t index = static_cast<uint8_t>(_config.dataRate);
  if (index >= (sizeof(timeTable) / sizeof(timeTable[0]))) {
    index = static_cast<uint8_t>(DataRate::SPS_128);
  }
  return timeTable[index];
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
  return result;
}

PollResult ADS1115::_finishJob(uint8_t instructionsUsed) {
  const bool finishedApply =
      _jobState == JobState::APPLY_WRITE_CONFIG ||
      _jobState == JobState::APPLY_VERIFY_CONFIG;
  if (finishedApply) {
    _clearHardwareConfigDirty();
  }

  _jobActive = false;
  _jobState = JobState::COMPLETE;
  _lastJobStatus = Status::Ok();
  _jobConfigRegister = 0;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMuxOverride = false;
  _jobNeedsReadback = false;
  _jobMux = _config.mux;
  return _pollResult(_lastJobStatus, instructionsUsed, true);
}

PollResult ADS1115::_failJob(const Status& status, uint8_t instructionsUsed) {
  const bool mayHaveTouchedConfig =
      _jobState == JobState::SINGLE_SHOT_WRITE_CONFIG ||
      _jobState == JobState::APPLY_WRITE_HIGH_THRESHOLD ||
      _jobState == JobState::APPLY_WRITE_CONFIG ||
      _jobState == JobState::APPLY_VERIFY_LOW_THRESHOLD ||
      _jobState == JobState::APPLY_VERIFY_HIGH_THRESHOLD ||
      _jobState == JobState::APPLY_VERIFY_CONFIG;
  if (instructionsUsed > 0 && mayHaveTouchedConfig && status.code != Err::OFFLINE) {
    _markHardwareConfigDirty(status);
  }

  _jobActive = false;
  _jobState = JobState::FAILED;
  _lastJobStatus = status;
  _jobConfigRegister = 0;
  _jobThresholdLow = 0;
  _jobThresholdHigh = 0;
  _jobMuxOverride = false;
  _jobNeedsReadback = false;
  _jobMux = _config.mux;
  return _pollResult(_lastJobStatus, instructionsUsed, true);
}

bool ADS1115::_isOffline() const {
  return _initialized && _driverState == DriverState::OFFLINE;
}

Status ADS1115::_offlineStatus() const {
  return Status::Error(Err::OFFLINE, "Driver is offline; call recover()",
                       _consecutiveFailures);
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
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen,
                              rxBuf, rxLen, _config.i2cTimeoutMs,
                              _config.i2cUser);
}

Status ADS1115::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  if (_config.i2cWrite == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback missing");
  }
  if (buf == nullptr || len == 0) {
    return Status::Error(Err::INVALID_PARAM, "Invalid I2C write parameters");
  }
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
}

Status ADS1115::_i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen,
                                     uint8_t* rxBuf, size_t rxLen) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return Status::Error(Err::OFFLINE, "Driver is offline; call recover()");
  }

  Status st = _i2cWriteReadRaw(txBuf, txLen, rxBuf, rxLen);
  if (st.code == Err::INVALID_CONFIG || st.code == Err::INVALID_PARAM) {
    return st;
  }
  return _updateHealth(st);
}

Status ADS1115::_i2cWriteTracked(const uint8_t* buf, size_t len) {
  if (_initialized && _driverState == DriverState::OFFLINE && !_allowOfflineI2c) {
    return Status::Error(Err::OFFLINE, "Driver is offline; call recover()");
  }

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

void ADS1115::_reassertOfflineLatch() {
  _driverState = DriverState::OFFLINE;
  const uint8_t threshold = _config.offlineThreshold == 0 ? 1 : _config.offlineThreshold;
  if (_consecutiveFailures < threshold) {
    _consecutiveFailures = threshold;
  }
}

// ============================================================================
// Internal
// ============================================================================

Status ADS1115::_applyConfig() {
  Status st = _writeRegister16Tracked(cmd::REG_LO_THRESH,
                                      static_cast<uint16_t>(_config.compThresholdLow));
  if (!st.ok()) {
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

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
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
    return st;
  }

  if (_config.mode == Mode::CONTINUOUS) {
    _conversionStarted = true;
    _conversionStartMs = (_config.nowMs != nullptr) ? _nowMs() : UINT32_MAX;
  } else {
    _conversionStarted = false;
    _conversionStartMs = 0;
  }
  _conversionReady = false;
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
}

void ADS1115::_clearHardwareConfigDirty() {
  _hardwareConfigDirty = false;
  _hardwareConfigDirtyError = Status::Ok();
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
