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

/// ADS1115 driver class
class ADS1115 {
public:
  // === Lifecycle ===
  Status begin(const Config& config);
  void tick(uint32_t nowMs);
  void end();

  // === Diagnostics (no health tracking) ===
  Status probe();
  Status recover();

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
  Status startConversion();
  Status startConversion(Mux mux);
  bool conversionReady();
  Status readRaw(int16_t& out);
  Status readVoltage(float& volts);
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);

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

  // === Comparator ===
  Status setThresholds(int16_t low, int16_t high);
  Status getThresholds(int16_t& low, int16_t& high);
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
  Status readRegister16(uint8_t reg, uint16_t& value);
  Status writeRegister16(uint8_t reg, uint16_t value);
  Status _readRegister16Raw(uint8_t reg, uint16_t& value);

  // === Health Tracking ===
  Status _updateHealth(const Status& st);

  // === Internal ===
  Status _applyConfig();
  uint16_t _buildConfigRegister() const;

  // === State ===
  Config _config;
  bool _initialized = false;
  DriverState _driverState = DriverState::UNINIT;

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
