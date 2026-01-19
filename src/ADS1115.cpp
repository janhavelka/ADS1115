/// @file ADS1115.cpp
/// @brief Implementation of ADS1115 driver

#include "ADS1115/ADS1115.h"

#include <Arduino.h>
#include <climits>

namespace ADS1115 {

namespace {

constexpr uint8_t kMinAddress = 0x48;
constexpr uint8_t kMaxAddress = 0x4B;

bool isValidMux(Mux mux) {
  return static_cast<uint8_t>(mux) <= static_cast<uint8_t>(Mux::AIN3_GND);
}

bool isValidGain(Gain gain) {
  return static_cast<uint8_t>(gain) <= static_cast<uint8_t>(Gain::FSR_0_256V);
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
  constexpr int16_t kAlertRdyLow = static_cast<int16_t>(0x0000);
  constexpr int16_t kAlertRdyHigh = static_cast<int16_t>(0x8000);
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

  return mux <= 7 && pga <= 5 && mode <= 1 && dr <= 7 &&
         compMode <= 1 && compPol <= 1 && compLat <= 1 && compQue <= 3;
}

} // namespace

// ============================================================================
// Lifecycle
// ============================================================================

Status ADS1115::begin(const Config& config) {
  _config = config;
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _conversionReady = false;
  _conversionStartMs = 0;
  _lastRawValue = 0;

  _lastOkMs = 0;
  _lastErrorMs = 0;
  _lastError = Status::Ok();
  _consecutiveFailures = 0;
  _totalFailures = 0;
  _totalSuccess = 0;

  if (_config.i2cWrite == nullptr || _config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C callbacks required");
  }
  if (_config.i2cTimeoutMs == 0) {
    return Status::Error(Err::INVALID_CONFIG, "Timeout must be > 0");
  }
  if (_config.i2cAddress < kMinAddress || _config.i2cAddress > kMaxAddress) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid I2C address");
  }
  if (!isValidMux(_config.mux) || !isValidGain(_config.gain) ||
      !isValidDataRate(_config.dataRate) || !isValidMode(_config.mode) ||
      !isValidCompMode(_config.compMode) || !isValidCompPolarity(_config.compPolarity) ||
      !isValidCompLatch(_config.compLatch) || !isValidCompQueue(_config.compQueue)) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid config enum value");
  }
  if (_config.alertRdyPin < -1) {
    return Status::Error(Err::INVALID_CONFIG, "Invalid ALERT/RDY pin");
  }
  if (_config.alertRdyPin >= 0 && _config.gpioRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "ALERT/RDY gpioRead required");
  }

  if (_config.offlineThreshold == 0) {
    _config.offlineThreshold = 1;
  }

  Status st = probe();
  if (!st.ok()) {
    return st;
  }

  st = _applyConfig();
  if (!st.ok()) {
    return st;
  }

  _initialized = true;
  _driverState = DriverState::READY;
  return Status::Ok();
}

void ADS1115::tick(uint32_t nowMs) {
  if (!_initialized) {
    return;
  }

  if (_config.mode == Mode::SINGLE_SHOT && _conversionStarted && !_conversionReady) {
    if ((nowMs - _conversionStartMs) >= getConversionTimeMs()) {
      if (useAlertRdyPin(_config)) {
        if (isAlertRdyAsserted(_config)) {
          _conversionStarted = false;
          _conversionReady = true;
        }
      } else {
        uint16_t configReg = 0;
        Status st = readRegister16(cmd::REG_CONFIG, configReg);
        if (st.ok() && ((configReg & cmd::MASK_OS) == cmd::OS_IDLE)) {
          _conversionStarted = false;
          _conversionReady = true;
        }
      }
    }
  }
}

void ADS1115::end() {
  _initialized = false;
  _driverState = DriverState::UNINIT;
  _conversionStarted = false;
  _conversionReady = false;
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
    return Status::Error(Err::DEVICE_NOT_FOUND, "ADS1115 not responding", st.detail);
  }
  return Status::Ok();
}

Status ADS1115::recover() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  uint16_t configReg = 0;
  return readRegister16(cmd::REG_CONFIG, configReg);
}

// ============================================================================
// Conversion API
// ============================================================================

Status ADS1115::startConversion() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return Status::Error(Err::BUSY, "Continuous mode active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  uint16_t configReg = _buildConfigRegister() | cmd::OS_START;
  Status st = writeRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = millis();
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
    return Status::Error(Err::BUSY, "Continuous mode active");
  }
  if (_conversionStarted) {
    return Status::Error(Err::BUSY, "Conversion already in progress");
  }

  Mux prevMux = _config.mux;
  _config.mux = mux;

  uint16_t configReg = _buildConfigRegister() | cmd::OS_START;
  Status st = writeRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    _config.mux = prevMux;
    return st;
  }

  _conversionStarted = true;
  _conversionReady = false;
  _conversionStartMs = millis();
  return Status{Err::IN_PROGRESS, 0, "Conversion started"};
}

bool ADS1115::conversionReady() {
  if (!_initialized) {
    return false;
  }
  if (_config.mode == Mode::CONTINUOUS) {
    return true;
  }
  if (_conversionReady) {
    return true;
  }
  if (!_conversionStarted) {
    return false;
  }

  if (useAlertRdyPin(_config)) {
    uint32_t nowMs = millis();
    if ((nowMs - _conversionStartMs) < getConversionTimeMs()) {
      return false;
    }
    if (isAlertRdyAsserted(_config)) {
      _conversionStarted = false;
      _conversionReady = true;
      return true;
    }
    return false;
  }

  uint32_t nowMs = millis();
  if ((nowMs - _conversionStartMs) < getConversionTimeMs()) {
    return false;
  }
  uint16_t configReg = 0;
  Status st = readRegister16(cmd::REG_CONFIG, configReg);
  if (!st.ok()) {
    return false;
  }

  if ((configReg & cmd::MASK_OS) == cmd::OS_IDLE) {
    _conversionStarted = false;
    _conversionReady = true;
    return true;
  }

  return false;
}

Status ADS1115::readRaw(int16_t& out) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  if (_config.mode == Mode::SINGLE_SHOT) {
    if (!_conversionReady) {
      if (_conversionStarted) {
        uint32_t nowMs = millis();
        if ((nowMs - _conversionStartMs) < getConversionTimeMs()) {
          return Status::Error(Err::CONVERSION_NOT_READY, "Conversion not ready");
        }
      }
      if (useAlertRdyPin(_config)) {
        if (!isAlertRdyAsserted(_config)) {
          return Status::Error(Err::CONVERSION_NOT_READY, "Conversion not ready");
        }
        _conversionStarted = false;
        _conversionReady = true;
      } else {
        uint16_t configReg = 0;
        Status st = readRegister16(cmd::REG_CONFIG, configReg);
        if (!st.ok()) {
          return st;
        }
        if ((configReg & cmd::MASK_OS) != cmd::OS_IDLE) {
          return Status::Error(Err::CONVERSION_NOT_READY, "Conversion not ready");
        }
        _conversionStarted = false;
        _conversionReady = true;
      }
    }
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
  if (_config.mode == Mode::CONTINUOUS) {
    return readRaw(out);
  }

  Status st = startConversion();
  if (!(st.code == Err::IN_PROGRESS || st.code == Err::BUSY)) {
    return st;
  }

  uint32_t startMs = (st.code == Err::BUSY) ? _conversionStartMs : millis();
  uint32_t deadlineMs = startMs + timeoutMs;
  uint32_t readyAtMs = startMs + getConversionTimeMs();

  while (static_cast<int32_t>(millis() - deadlineMs) < 0) {
    uint32_t nowMs = millis();
    if (static_cast<int32_t>(nowMs - readyAtMs) < 0) {
      continue;
    }

    Status readSt = readRaw(out);
    if (readSt.ok()) {
      return Status::Ok();
    }
    if (readSt.code != Err::CONVERSION_NOT_READY) {
      return readSt;
    }
  }

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
  _config.mux = mux;
  return _applyConfig();
}

Status ADS1115::setGain(Gain gain) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidGain(gain)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid gain");
  }
  _config.gain = gain;
  return _applyConfig();
}

Status ADS1115::setDataRate(DataRate rate) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidDataRate(rate)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid data rate");
  }
  _config.dataRate = rate;
  return _applyConfig();
}

Status ADS1115::setMode(Mode mode) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidMode(mode)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid mode");
  }
  _config.mode = mode;
  _conversionStarted = false;
  _conversionReady = false;
  return _applyConfig();
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

  Status st = writeRegister16(cmd::REG_CONFIG, config);
  if (!st.ok()) {
    return st;
  }

  _config.mux = static_cast<Mux>((config & cmd::MASK_MUX) >> cmd::BIT_MUX);
  _config.gain = static_cast<Gain>((config & cmd::MASK_PGA) >> cmd::BIT_PGA);
  _config.mode = static_cast<Mode>((config & cmd::MASK_MODE) >> cmd::BIT_MODE);
  _config.dataRate = static_cast<DataRate>((config & cmd::MASK_DR) >> cmd::BIT_DR);
  _config.compMode = static_cast<ComparatorMode>((config & cmd::MASK_COMP_MODE) >> cmd::BIT_COMP_MODE);
  _config.compPolarity = static_cast<ComparatorPolarity>((config & cmd::MASK_COMP_POL) >> cmd::BIT_COMP_POL);
  _config.compLatch = static_cast<ComparatorLatch>((config & cmd::MASK_COMP_LAT) >> cmd::BIT_COMP_LAT);
  _config.compQueue = static_cast<ComparatorQueue>((config & cmd::MASK_COMP_QUE) >> cmd::BIT_COMP_QUE);

  if (_config.mode == Mode::SINGLE_SHOT && ((config & cmd::MASK_OS) == cmd::OS_START)) {
    _conversionStarted = true;
    _conversionReady = false;
    _conversionStartMs = millis();
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

  _config.compThresholdLow = low;
  _config.compThresholdHigh = high;

  Status st = writeRegister16(cmd::REG_LO_THRESH, static_cast<uint16_t>(low));
  if (!st.ok()) {
    return st;
  }
  return writeRegister16(cmd::REG_HI_THRESH, static_cast<uint16_t>(high));
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
  _config.compMode = mode;
  return _applyConfig();
}

Status ADS1115::setComparatorPolarity(ComparatorPolarity polarity) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompPolarity(polarity)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator polarity");
  }
  _config.compPolarity = polarity;
  return _applyConfig();
}

Status ADS1115::setComparatorLatch(ComparatorLatch latch) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompLatch(latch)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator latch");
  }
  _config.compLatch = latch;
  return _applyConfig();
}

Status ADS1115::setComparatorQueue(ComparatorQueue queue) {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }
  if (!isValidCompQueue(queue)) {
    return Status::Error(Err::INVALID_PARAM, "Invalid comparator queue");
  }
  _config.compQueue = queue;
  return _applyConfig();
}

Status ADS1115::enableConversionReadyPin() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  _config.compThresholdLow = 0x0000;
  _config.compThresholdHigh = 0x8000;
  _config.compQueue = ComparatorQueue::ASSERT_1;
  _config.compMode = ComparatorMode::TRADITIONAL;
  _config.compLatch = ComparatorLatch::NON_LATCHING;

  return _applyConfig();
}

Status ADS1115::disableComparator() {
  if (!_initialized) {
    return Status::Error(Err::NOT_INITIALIZED, "Driver not initialized");
  }

  _config.compQueue = ComparatorQueue::DISABLE;
  return _applyConfig();
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

// ============================================================================
// Transport Wrappers
// ============================================================================

Status ADS1115::_i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen,
                                 uint8_t* rxBuf, size_t rxLen) {
  if (_config.i2cWriteRead == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C read callback missing");
  }
  return _config.i2cWriteRead(_config.i2cAddress, txBuf, txLen,
                              rxBuf, rxLen, _config.i2cTimeoutMs,
                              _config.i2cUser);
}

Status ADS1115::_i2cWriteRaw(const uint8_t* buf, size_t len) {
  if (_config.i2cWrite == nullptr) {
    return Status::Error(Err::INVALID_CONFIG, "I2C write callback missing");
  }
  return _config.i2cWrite(_config.i2cAddress, buf, len,
                          _config.i2cTimeoutMs, _config.i2cUser);
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
  uint8_t rx[2] = {0, 0};
  Status st = _i2cWriteReadTracked(&reg, 1, rx, sizeof(rx));
  if (!st.ok()) {
    return st;
  }
  value = (static_cast<uint16_t>(rx[0]) << 8) | rx[1];
  return Status::Ok();
}

Status ADS1115::writeRegister16(uint8_t reg, uint16_t value) {
  uint8_t tx[3] = {
    reg,
    static_cast<uint8_t>((value >> 8) & 0xFF),
    static_cast<uint8_t>(value & 0xFF)
  };
  return _i2cWriteTracked(tx, sizeof(tx));
}

Status ADS1115::_readRegister16Raw(uint8_t reg, uint16_t& value) {
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
  uint32_t nowMs = millis();

  if (st.ok() || st.inProgress()) {
    _lastOkMs = nowMs;
    _consecutiveFailures = 0;
    if (_totalSuccess < UINT32_MAX) {
      _totalSuccess++;
    }

    if (_initialized) {
      _driverState = DriverState::READY;
    }
  } else {
    _lastErrorMs = nowMs;
    _lastError = st;

    if (_consecutiveFailures < UINT8_MAX) {
      _consecutiveFailures++;
    }
    if (_totalFailures < UINT32_MAX) {
      _totalFailures++;
    }

    if (_initialized) {
      if (_consecutiveFailures >= _config.offlineThreshold) {
        _driverState = DriverState::OFFLINE;
      } else {
        _driverState = DriverState::DEGRADED;
      }
    }
  }

  return st;
}

// ============================================================================
// Internal
// ============================================================================

Status ADS1115::_applyConfig() {
  Status st = writeRegister16(cmd::REG_LO_THRESH,
                              static_cast<uint16_t>(_config.compThresholdLow));
  if (!st.ok()) {
    return st;
  }
  st = writeRegister16(cmd::REG_HI_THRESH,
                       static_cast<uint16_t>(_config.compThresholdHigh));
  if (!st.ok()) {
    return st;
  }
  st = writeRegister16(cmd::REG_CONFIG, _buildConfigRegister());
  if (!st.ok()) {
    return st;
  }

  _conversionStarted = false;
  _conversionReady = false;
  return Status::Ok();
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

} // namespace ADS1115
