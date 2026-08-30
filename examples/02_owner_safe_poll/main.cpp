/// @file main.cpp
/// @brief Externally serialized, transaction-budgeted ADS1115 owner loop.
///
/// This example intentionally keeps the bus, mutex, pins, timeout policy, and
/// scheduling in application code. Each loop pass advances at most one I2C
/// callback and consumes terminal results by operation token.

#include <Arduino.h>
#include <Wire.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ADS1115/ADS1115.h"

namespace {

constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
constexpr uint32_t I2C_CLOCK_HZ = 400000;
constexpr uint32_t TRANSFER_TIMEOUT_MS = 20;
constexpr uint32_t OWNER_SCHEDULING_MARGIN_MS = 5;
// Start write, initial OS read, one bounded OS retry, and conversion read.
constexpr uint32_t READ_CALLBACK_COUNT = 4;
constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
constexpr uint32_t RECOVERY_DEADLINE_MS = 200;
constexpr uint8_t MAX_RECOVERY_ATTEMPTS = 2;

struct SharedBusOwner {
  TwoWire* wire = nullptr;
  SemaphoreHandle_t mutex = nullptr;
};

StaticSemaphore_t busMutexStorage;
SharedBusOwner busOwner{&Wire, nullptr};
ADS1115::ADS1115 adc;
ADS1115::DeviceProfile profile;
ADS1115::OperationToken activeToken;
uint32_t nextSampleAtMs = 0;
uint8_t recoveryAttempts = 0;

enum class AppState : uint8_t {
  INITIALIZING,
  IDLE,
  READING,
  RECOVERING,
  FAILED
};

AppState appState = AppState::FAILED;

ADS1115::Status mapWireStatus(uint8_t result) {
  switch (result) {
    case 0:
      return ADS1115::Status::Ok();
    case 1:
      return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM,
                                    "Wire buffer rejected transfer", result);
    case 2:
      return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_ADDR,
                                    "I2C address NACK", result);
    case 3:
      return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_DATA,
                                    "I2C data NACK", result);
    case 4:
      return ADS1115::Status::Error(ADS1115::Err::I2C_BUS,
                                    "I2C bus error", result);
    case 5:
      return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT,
                                    "I2C timeout", result);
    default:
      return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR,
                                    "Unknown Wire error", result);
  }
}

ADS1115::Status lockBus(SharedBusOwner& owner, uint32_t timeoutMs,
                        uint32_t& remainingMs) {
  remainingMs = 0;
  if (owner.wire == nullptr || owner.mutex == nullptr || timeoutMs == 0) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG,
                                  "Invalid shared-bus owner");
  }

  const uint32_t startedAtMs = millis();
  TickType_t ticks = pdMS_TO_TICKS(timeoutMs);
  if (ticks == 0) {
    ticks = 1;
  }
  if (xSemaphoreTake(owner.mutex, ticks) != pdTRUE) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT,
                                  "Shared-bus lock timeout");
  }

  const uint32_t elapsedMs = millis() - startedAtMs;
  if (elapsedMs >= timeoutMs) {
    xSemaphoreGive(owner.mutex);
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT,
                                  "Shared-bus deadline exhausted");
  }
  remainingMs = timeoutMs - elapsedMs;
  return ADS1115::Status::Ok();
}

struct ScopedBusUnlock {
  explicit ScopedBusUnlock(SharedBusOwner& ownerIn) : owner(ownerIn) {}
  ~ScopedBusUnlock() { xSemaphoreGive(owner.mutex); }
  SharedBusOwner& owner;
};

ADS1115::Status ownerWrite(uint8_t address, const uint8_t* data, size_t length,
                           uint32_t timeoutMs, void* user) {
  if (data == nullptr || length == 0 || user == nullptr) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM,
                                  "Invalid write request");
  }

  SharedBusOwner& owner = *static_cast<SharedBusOwner*>(user);
  uint32_t remainingMs = 0;
  ADS1115::Status status = lockBus(owner, timeoutMs, remainingMs);
  if (!status.ok()) {
    return status;
  }
  ScopedBusUnlock unlock(owner);

  owner.wire->setTimeOut(remainingMs);
  owner.wire->beginTransmission(address);
  if (owner.wire->write(data, length) != length) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR,
                                  "Incomplete Wire write");
  }
  return mapWireStatus(owner.wire->endTransmission(true));
}

ADS1115::Status ownerWriteRead(uint8_t address, const uint8_t* tx,
                               size_t txLength, uint8_t* rx, size_t rxLength,
                               uint32_t timeoutMs, void* user) {
  if (tx == nullptr || txLength == 0 || rx == nullptr || rxLength == 0 ||
      rxLength > UINT8_MAX || user == nullptr) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM,
                                  "Invalid write-read request");
  }

  const uint32_t callbackStartedAtMs = millis();
  SharedBusOwner& owner = *static_cast<SharedBusOwner*>(user);
  uint32_t remainingMs = 0;
  ADS1115::Status status = lockBus(owner, timeoutMs, remainingMs);
  if (!status.ok()) {
    return status;
  }
  ScopedBusUnlock unlock(owner);

  owner.wire->setTimeOut(remainingMs);
  owner.wire->beginTransmission(address);
  if (owner.wire->write(tx, txLength) != txLength) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR,
                                  "Incomplete Wire pointer write");
  }
  status = mapWireStatus(owner.wire->endTransmission(false));
  if (!status.ok()) {
    return status;
  }

  const uint32_t elapsedMs = millis() - callbackStartedAtMs;
  if (elapsedMs >= timeoutMs) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT,
                                  "Write-read deadline exhausted");
  }
  remainingMs = timeoutMs - elapsedMs;
  owner.wire->setTimeOut(remainingMs);

  const size_t count = owner.wire->requestFrom(
      address, static_cast<uint8_t>(rxLength));
  if (count != rxLength) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR,
                                  "Wire read length mismatch",
                                  static_cast<int32_t>(count));
  }
  for (size_t i = 0; i < rxLength; ++i) {
    rx[i] = static_cast<uint8_t>(owner.wire->read());
  }
  return ADS1115::Status::Ok();
}

void fail(const ADS1115::Status& status) {
  Serial.printf("ADS1115 error code=%u detail=%ld: %s\n",
                static_cast<unsigned>(status.code),
                static_cast<long>(status.detail), status.msg);
  appState = AppState::FAILED;
}

void startRecovery(uint32_t nowMs, const ADS1115::Status& cause) {
  Serial.printf("ADS1115 operation failed code=%u detail=%ld: %s\n",
                static_cast<unsigned>(cause.code),
                static_cast<long>(cause.detail), cause.msg);
  if (recoveryAttempts >= MAX_RECOVERY_ATTEMPTS) {
    fail(ADS1115::Status::Error(ADS1115::Err::OFFLINE,
                                "Bounded recovery attempts exhausted",
                                recoveryAttempts));
    return;
  }

  recoveryAttempts++;
  ADS1115::Status status = adc.startRecover(
      nowMs, nowMs + RECOVERY_DEADLINE_MS, activeToken);
  if (!status.inProgress()) {
    fail(status);
    return;
  }
  appState = AppState::RECOVERING;
}

void startInitialization(uint32_t nowMs) {
  ADS1115::Status status = adc.startInitialize(nowMs, nowMs + 200U, activeToken);
  if (!status.inProgress()) {
    fail(status);
    return;
  }
  appState = AppState::INITIALIZING;
}

void startSample(uint32_t nowMs) {
  ADS1115::ChannelRequest request;
  request.channelId = 1;
  request.mux = ADS1115::Mux::AIN0_GND;
  request.gain = ADS1115::Gain::FSR_2_048V;

  const uint32_t durationMs = ADS1115::operationDeadlineMs(
      1, profile.dataRate,
      READ_CALLBACK_COUNT * TRANSFER_TIMEOUT_MS + OWNER_SCHEDULING_MARGIN_MS);
  ADS1115::Status status = adc.startRead(
      request, nowMs, nowMs + durationMs, activeToken);
  if (!status.inProgress()) {
    startRecovery(nowMs, status);
    return;
  }
  appState = AppState::READING;
}

void consumeTerminalResult(uint32_t nowMs) {
  ADS1115::OperationResult result;
  ADS1115::Status status = adc.takeResult(activeToken, result);
  activeToken = ADS1115::OperationToken{};
  if (!status.ok()) {
    fail(status);
    return;
  }
  if (!result.status.ok()) {
    startRecovery(nowMs, result.status);
    return;
  }

  if (result.kind == ADS1115::OperationKind::INITIALIZE ||
      result.kind == ADS1115::OperationKind::RECOVER) {
    recoveryAttempts = 0;
    appState = AppState::IDLE;
    nextSampleAtMs = nowMs;
    return;
  }
  if (result.kind == ADS1115::OperationKind::READ_SINGLE_SHOT &&
      result.sampleValid) {
    Serial.printf("channel=%u raw=%d uV=%ld cfg=%lu seq=%lu\n",
                  result.sample.channelId, result.sample.rawCode,
                  static_cast<long>(result.sample.microvolts),
                  static_cast<unsigned long>(result.sample.configGeneration),
                  static_cast<unsigned long>(result.sample.sequence));
    recoveryAttempts = 0;
    appState = AppState::IDLE;
    nextSampleAtMs = nowMs + SAMPLE_INTERVAL_MS;
    return;
  }

  fail(ADS1115::Status::Error(ADS1115::Err::INDETERMINATE,
                              "Unexpected terminal result"));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  if (!Wire.begin(I2C_SDA, I2C_SCL)) {
    fail(ADS1115::Status::Error(ADS1115::Err::I2C_BUS,
                                "Wire initialization failed"));
    return;
  }
  if (!Wire.setClock(I2C_CLOCK_HZ)) {
    fail(ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG,
                                "Wire clock configuration failed"));
    return;
  }

  busOwner.mutex = xSemaphoreCreateMutexStatic(&busMutexStorage);

  ADS1115::DriverConfig driverConfig;
  driverConfig.i2cWrite = ownerWrite;
  driverConfig.i2cWriteRead = ownerWriteRead;
  driverConfig.i2cUser = &busOwner;
  driverConfig.transferTimeoutMs = TRANSFER_TIMEOUT_MS;

  profile.i2cAddress = 0x48;
  profile.defaultMux = ADS1115::Mux::AIN0_GND;
  profile.defaultGain = ADS1115::Gain::FSR_2_048V;
  profile.dataRate = ADS1115::DataRate::SPS_128;
  profile.mode = ADS1115::Mode::SINGLE_SHOT;
  profile.comparator.use = ADS1115::ComparatorUse::OFF;

  const ADS1115::Status status = adc.bind(driverConfig, profile);
  if (!status.ok()) {
    fail(status);
    return;
  }
  startInitialization(millis());
}

void loop() {
  const uint32_t nowMs = millis();
  if (appState == AppState::FAILED) {
    return;
  }

  if (activeToken.valid()) {
    const ADS1115::PollResult progress = adc.poll(nowMs, 1);
    if (progress.done) {
      consumeTerminalResult(nowMs);
    }
    return;
  }

  if (appState == AppState::IDLE &&
      static_cast<int32_t>(nowMs - nextSampleAtMs) >= 0) {
    startSample(nowMs);
  }
}
