/// @file test_basic.cpp
/// @brief Native contract tests for ADS1115 lifecycle and health behavior.

#include <unity.h>

#include "Arduino.h"
#include "Wire.h"

SerialClass Serial;
TwoWire Wire;

#define private public
#include "ADS1115/ADS1115.h"
#undef private

using namespace ADS1115;

namespace {

struct FakeBus {
  struct RegisterWrite {
    uint8_t reg = 0;
    uint16_t value = 0;
  };

  Status writeStatus = Status::Ok();
  Status readStatus = Status::Ok();
  Status failStatus = Status::Error(Err::I2C_BUS, "forced transport failure", -99);
  uint16_t regs[4] = {
    cmd::CONVERSION_DEFAULT,
    cmd::CONFIG_DEFAULT,
    cmd::LO_THRESH_DEFAULT,
    cmd::HI_THRESH_DEFAULT
  };
  uint32_t nowMs = 1234;
  uint32_t writeCalls = 0;
  uint32_t readCalls = 0;
  uint32_t transportCalls = 0;
  uint32_t failOnCall = 0;
  uint16_t busyConfigReadsRemaining = 0;
  uint8_t lastWriteReg = 0xFF;
  uint8_t lastReadReg = 0xFF;
  uint32_t writesByReg[4] = {0, 0, 0, 0};
  uint32_t readsByReg[4] = {0, 0, 0, 0};
  RegisterWrite writeLog[32] = {};
  uint8_t writeLogCount = 0;
  uint32_t yieldCalls = 0;
  uint32_t yieldAdvanceMs = 0;

  uint32_t instructionCalls() const {
    return writeCalls + readCalls;
  }

  void failTransferAfter(uint32_t transferOffset, Status status) {
    failOnCall = transportCalls + transferOffset;
    failStatus = status;
  }

  void failNextTransfer(Status status) {
    failTransferAfter(1, status);
  }

  void clearTransferFailure() {
    failOnCall = 0;
    failStatus = Status::Error(Err::I2C_BUS, "forced transport failure", -99);
  }

  void resetCounts() {
    writeCalls = 0;
    readCalls = 0;
    transportCalls = 0;
    lastWriteReg = 0xFF;
    lastReadReg = 0xFF;
    for (uint8_t i = 0; i < 4; ++i) {
      writesByReg[i] = 0;
      readsByReg[i] = 0;
    }
    writeLogCount = 0;
    yieldCalls = 0;
  }
};

Status fakeWrite(uint8_t, const uint8_t* data, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  bus->transportCalls++;
  if (data == nullptr || len < 3) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C write");
  }
  const uint8_t reg = data[0];
  const uint16_t value = (static_cast<uint16_t>(data[1]) << 8) | data[2];
  bus->lastWriteReg = reg;
  if (bus->failOnCall != 0 && bus->transportCalls == bus->failOnCall) {
    return bus->failStatus;
  }
  if (!bus->writeStatus.ok()) {
    return bus->writeStatus;
  }
  if (reg < 4) {
    if (bus->writeLogCount < (sizeof(bus->writeLog) / sizeof(bus->writeLog[0]))) {
      bus->writeLog[bus->writeLogCount].reg = reg;
      bus->writeLog[bus->writeLogCount].value = value;
      bus->writeLogCount++;
    }
    bus->writesByReg[reg]++;
    bus->regs[reg] = value;
  }
  return bus->writeStatus;
}

Status fakeWriteRead(uint8_t, const uint8_t* txData, size_t txLen, uint8_t* rxData,
                     size_t rxLen, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->readCalls++;
  bus->transportCalls++;
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C buffers");
  }
  const uint8_t reg = txData[0];
  bus->lastReadReg = reg;
  if (bus->failOnCall != 0 && bus->transportCalls == bus->failOnCall) {
    return bus->failStatus;
  }
  if (!bus->readStatus.ok()) {
    return bus->readStatus;
  }
  uint16_t value = 0;
  if (reg < 4) {
    bus->readsByReg[reg]++;
    value = bus->regs[reg];
    if (reg == cmd::REG_CONFIG) {
      if (bus->busyConfigReadsRemaining > 0) {
        bus->busyConfigReadsRemaining--;
        value &= static_cast<uint16_t>(~cmd::MASK_OS);
      } else {
        value |= cmd::OS_IDLE;
      }
    }
  }
  if (rxLen >= 1) {
    rxData[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
  }
  if (rxLen >= 2) {
    rxData[1] = static_cast<uint8_t>(value & 0xFF);
  }
  for (size_t i = 2; i < rxLen; ++i) {
    rxData[i] = 0;
  }
  return Status::Ok();
}

uint32_t fakeNowMs(void* user) {
  return static_cast<FakeBus*>(user)->nowMs;
}

bool fakeGpioRead(int, void*) {
  return true;
}

void fakeYield(void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->yieldCalls++;
  bus->nowMs += bus->yieldAdvanceMs;
}

Config makeConfig(FakeBus& bus) {
  Config cfg;
  cfg.i2cWrite = fakeWrite;
  cfg.i2cWriteRead = fakeWriteRead;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
  cfg.timeUser = &bus;
  cfg.offlineThreshold = 3;
  cfg.i2cTimeoutMs = 10;
  return cfg;
}

void assertStatusEqual(const Status& expected, const Status& actual) {
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(static_cast<uint8_t>(expected.code),
                                  static_cast<uint8_t>(actual.code),
                                  expected.msg);
  TEST_ASSERT_EQUAL_INT32_MESSAGE(expected.detail, actual.detail, expected.msg);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(expected.msg, actual.msg, expected.msg);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_status_ok() {
  Status st = Status::Ok();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(static_cast<bool>(st));
  TEST_ASSERT_TRUE(st.is(Err::OK));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OK), static_cast<uint8_t>(st.code));
}

void test_status_error() {
  Status st = Status::Error(Err::I2C_ERROR, "Test error", 42);
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_FALSE(static_cast<bool>(st));
  TEST_ASSERT_TRUE(st.is(Err::I2C_ERROR));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(42, st.detail);
}

void test_status_in_progress() {
  Status st{Err::IN_PROGRESS, 0, "In progress"};
  TEST_ASSERT_FALSE(st.ok());
  TEST_ASSERT_FALSE(static_cast<bool>(st));
  TEST_ASSERT_TRUE(st.is(Err::IN_PROGRESS));
  TEST_ASSERT_TRUE(st.inProgress());
}

void test_config_defaults() {
  Config cfg;
  TEST_ASSERT_NULL(cfg.i2cWrite);
  TEST_ASSERT_NULL(cfg.i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x48, cfg.i2cAddress);
  TEST_ASSERT_EQUAL_UINT16(50, cfg.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN0_GND), static_cast<uint8_t>(cfg.mux));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_2_048V), static_cast<uint8_t>(cfg.gain));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DataRate::SPS_128), static_cast<uint8_t>(cfg.dataRate));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SINGLE_SHOT), static_cast<uint8_t>(cfg.mode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ComparatorQueue::DISABLE), static_cast<uint8_t>(cfg.compQueue));
  TEST_ASSERT_EQUAL_INT16(0x7FFF, cfg.compThresholdHigh);
  TEST_ASSERT_EQUAL_INT16(static_cast<int16_t>(0x8000), cfg.compThresholdLow);
  TEST_ASSERT_EQUAL_UINT8(5, cfg.offlineThreshold);
}

void test_get_settings_snapshot() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x4B;
  cfg.mux = Mux::AIN1_GND;
  cfg.gain = Gain::FSR_0_512V;
  cfg.dataRate = DataRate::SPS_475;
  cfg.mode = Mode::CONTINUOUS;
  cfg.compMode = ComparatorMode::WINDOW;
  cfg.compPolarity = ComparatorPolarity::ACTIVE_HIGH;
  cfg.compLatch = ComparatorLatch::LATCHING;
  cfg.compQueue = ComparatorQueue::ASSERT_2;
  cfg.alertRdyPin = 17;
  cfg.gpioRead = fakeGpioRead;
  cfg.cooperativeYield = fakeYield;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  SettingsSnapshot snap;
  Status st = dev.getSettings(snap);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(snap.state));
  TEST_ASSERT_EQUAL_HEX8(0x4B, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(10u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_TRUE(snap.hasGpioReadHook);
  TEST_ASSERT_TRUE(snap.hasCooperativeYieldHook);
  TEST_ASSERT_EQUAL(17, snap.alertRdyPin);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN1_GND),
                          static_cast<uint8_t>(snap.mux));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_512V),
                          static_cast<uint8_t>(snap.gain));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DataRate::SPS_475),
                          static_cast<uint8_t>(snap.dataRate));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::CONTINUOUS),
                          static_cast<uint8_t>(snap.mode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ComparatorMode::WINDOW),
                          static_cast<uint8_t>(snap.compMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ComparatorPolarity::ACTIVE_HIGH),
                          static_cast<uint8_t>(snap.compPolarity));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ComparatorLatch::LATCHING),
                          static_cast<uint8_t>(snap.compLatch));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ComparatorQueue::ASSERT_2),
                          static_cast<uint8_t>(snap.compQueue));
  TEST_ASSERT_FALSE(snap.conversionStarted);
  TEST_ASSERT_FALSE(snap.conversionReady);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.conversionStartMs);
  TEST_ASSERT_EQUAL_INT16(0, snap.lastRawValue);
  TEST_ASSERT_FALSE(snap.hardwareConfigDirty);
  TEST_ASSERT_FALSE(snap.hardwareConfigUncertain);
  TEST_ASSERT_TRUE(snap.lastConfigApplyError.ok());
  TEST_ASSERT_FALSE(dev.isHardwareConfigDirty());
  TEST_ASSERT_FALSE(dev.isHardwareConfigUncertain());
}

void test_begin_rejects_missing_callbacks() {
  ADS1115::ADS1115 dev;
  Config cfg;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_begin_rejects_missing_now_ms() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.instructionCalls());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_begin_success_sets_ready_and_counters() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.isOnline());
  TEST_ASSERT_EQUAL_UINT32(3u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastOkMs());
}

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = dev.totalSuccess();
  const uint32_t beforeFailures = dev.totalFailures();
  const uint8_t beforeConsecutive = dev.consecutiveFailures();
  const DriverState beforeState = dev.state();

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced probe timeout", -7);
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(beforeConsecutive, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeState),
                          static_cast<uint8_t>(dev.state()));
}

void test_recover_failure_updates_health() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced recover timeout", -9);
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastErrorMs());
}

void test_recover_success_returns_ready() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced recover timeout", -9);
  (void)dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));

  const uint32_t successBefore = dev.totalSuccess();
  bus.nowMs = 4321;
  bus.readStatus = Status::Ok();
  Status st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_GREATER_THAN_UINT32(successBefore, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(4321u, dev.lastOkMs());
}

void test_recover_reaches_offline_when_threshold_is_one() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -11);
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_FALSE(dev.isOnline());
}

void test_tracked_transport_errors_propagate_and_drive_health_state() {
  struct FailureCase {
    Status status;
    bool write;
    DriverState expectedState;
    uint8_t expectedConsecutive;
  };

  static const FailureCase failures[] = {
    {Status::Error(Err::I2C_TIMEOUT, "tracked timeout", -10), false,
     DriverState::DEGRADED, 1},
    {Status::Error(Err::I2C_NACK_ADDR, "tracked address nack", -11), false,
     DriverState::DEGRADED, 2},
    {Status::Error(Err::I2C_NACK_DATA, "tracked data nack", -12), true,
     DriverState::DEGRADED, 3},
    {Status::Error(Err::I2C_BUS, "tracked bus error", -13), true,
     DriverState::OFFLINE, 4},
  };

  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 4;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  const uint32_t successBefore = dev.totalSuccess();
  for (size_t i = 0; i < (sizeof(failures) / sizeof(failures[0])); ++i) {
    bus.nowMs += 10;
    bus.failNextTransfer(failures[i].status);

    uint16_t value = 0;
    Status st = failures[i].write
                    ? dev.writeRegister16(cmd::REG_CONFIG, cmd::CONFIG_DEFAULT)
                    : dev.readRegister16(cmd::REG_CONFIG, value);

    assertStatusEqual(failures[i].status, st);
    assertStatusEqual(failures[i].status, dev.lastError());
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(i + 1), dev.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(failures[i].expectedConsecutive,
                            dev.consecutiveFailures());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(failures[i].expectedState),
                            static_cast<uint8_t>(dev.state()));
    TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastErrorMs());
    TEST_ASSERT_EQUAL_UINT32(successBefore, dev.totalSuccess());
  }

  bus.resetCounts();
  uint16_t blockedValue = 0;
  Status blocked = dev.readRegister16(cmd::REG_CONFIG, blockedValue);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE),
                          static_cast<uint8_t>(blocked.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.instructionCalls());

  bus.clearTransferFailure();
  bus.nowMs += 10;
  Status st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(4u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT32(successBefore + 4u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastOkMs());
}

void test_single_shot_timing_wraparound_reaches_ready() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.nowMs = 0xFFFFFFF8u;
  Status st = dev.startConversion();
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_FALSE(dev.conversionReady());

  // 13 ms elapsed across wrap (0xFFFFFFF8 -> 0x00000005), which is enough for 128 SPS.
  bus.nowMs = 5u;
  dev.tick(bus.nowMs);
  TEST_ASSERT_TRUE(dev.conversionReady());

  int16_t raw = 0;
  st = dev.readRaw(raw);
  TEST_ASSERT_TRUE(st.ok());
}

void test_read_blocking_timeout_is_bounded_and_clears_conversion_state() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.dataRate = DataRate::SPS_128;
  cfg.cooperativeYield = fakeYield;
  bus.nowMs = 1000;
  bus.yieldAdvanceMs = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  bus.resetCounts();

  int16_t raw = 123;
  Status st = dev.readBlocking(raw, 5);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(5u, bus.yieldCalls);
  TEST_ASSERT_EQUAL_UINT32(1005u, bus.nowMs);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writesByReg[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readsByReg[cmd::REG_CONVERSION]);

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.conversionStarted);
  TEST_ASSERT_FALSE(snap.conversionReady);
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
}

void test_begin_partial_apply_failure_leaves_partial_register_state() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mux = Mux::AIN3_GND;
  cfg.gain = Gain::FSR_0_512V;
  cfg.dataRate = DataRate::SPS_860;
  cfg.compThresholdLow = 0x1234;
  cfg.compThresholdHigh = 0x2345;
  const Status expected =
      Status::Error(Err::I2C_NACK_DATA, "begin config write nack", -20);

  bus.failTransferAfter(4, expected);  // probe, low threshold, high threshold, config
  Status st = dev.begin(cfg);

  assertStatusEqual(expected, st);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(2u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  assertStatusEqual(expected, dev.lastError());
  TEST_ASSERT_TRUE(dev.isHardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.isHardwareConfigUncertain());
  assertStatusEqual(expected, dev.lastConfigApplyError());
  TEST_ASSERT_EQUAL_HEX16(0x1234, bus.regs[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_HEX16(0x2345, bus.regs[cmd::REG_HI_THRESH]);
  TEST_ASSERT_EQUAL_HEX16(cmd::CONFIG_DEFAULT, bus.regs[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT8(2u, bus.writeLogCount);
  TEST_ASSERT_EQUAL_UINT8(cmd::REG_LO_THRESH, bus.writeLog[0].reg);
  TEST_ASSERT_EQUAL_UINT8(cmd::REG_HI_THRESH, bus.writeLog[1].reg);
  TEST_ASSERT_NOT_EQUAL(dev._buildConfigRegister(), bus.regs[cmd::REG_CONFIG]);

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_512V),
                          static_cast<uint8_t>(snap.gain));
  TEST_ASSERT_TRUE(snap.hardwareConfigDirty);
  TEST_ASSERT_TRUE(snap.hardwareConfigUncertain);
  assertStatusEqual(expected, snap.lastConfigApplyError);
}

void test_recover_partial_apply_failure_preserves_mismatch_until_retry() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.gain = Gain::FSR_0_512V;
  cfg.dataRate = DataRate::SPS_860;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.regs[cmd::REG_CONFIG] = cmd::CONFIG_DEFAULT;
  TEST_ASSERT_NOT_EQUAL(dev._buildConfigRegister(), bus.regs[cmd::REG_CONFIG]);
  bus.resetCounts();

  const Status expected =
      Status::Error(Err::I2C_BUS, "recover config write bus error", -21);
  bus.failTransferAfter(4, expected);  // read config, low threshold, high threshold, config
  Status st = dev.recover();
  assertStatusEqual(expected, st);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_TRUE(dev.isHardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.isHardwareConfigUncertain());
  assertStatusEqual(expected, dev.lastConfigApplyError());
  TEST_ASSERT_NOT_EQUAL(dev._buildConfigRegister(), bus.regs[cmd::REG_CONFIG]);

  bus.clearTransferFailure();
  bus.nowMs += 10;
  st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX16(dev._buildConfigRegister(), bus.regs[cmd::REG_CONFIG]);
  TEST_ASSERT_FALSE(dev.isHardwareConfigDirty());
  TEST_ASSERT_FALSE(dev.isHardwareConfigUncertain());
  TEST_ASSERT_TRUE(dev.lastConfigApplyError().ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
}

void test_setter_partial_apply_mismatch_recover_clears() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint16_t beforeConfig = bus.regs[cmd::REG_CONFIG];
  bus.resetCounts();

  const Status expected =
      Status::Error(Err::I2C_NACK_DATA, "setter config write nack", -22);
  bus.failTransferAfter(3, expected);  // low threshold, high threshold, config
  Status st = dev.setGain(Gain::FSR_0_512V);

  assertStatusEqual(expected, st);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_512V),
                          static_cast<uint8_t>(dev.getGain()));
  TEST_ASSERT_TRUE(dev.isHardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.isHardwareConfigUncertain());
  assertStatusEqual(expected, dev.lastConfigApplyError());
  TEST_ASSERT_EQUAL_HEX16(beforeConfig, bus.regs[cmd::REG_CONFIG]);
  TEST_ASSERT_NOT_EQUAL(dev._buildConfigRegister(), bus.regs[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writesByReg[cmd::REG_CONFIG]);

  bus.clearTransferFailure();
  bus.nowMs += 10;
  st = dev.recover();
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_HEX16(dev._buildConfigRegister(), bus.regs[cmd::REG_CONFIG]);
  TEST_ASSERT_FALSE(dev.isHardwareConfigDirty());
  TEST_ASSERT_FALSE(dev.isHardwareConfigUncertain());
  TEST_ASSERT_TRUE(dev.lastConfigApplyError().ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
}

void test_raw_transport_rejects_invalid_buffers() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  uint8_t byte = 0;
  uint8_t rx = 0;

  Status st = dev._i2cWriteRaw(nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteRaw(&byte, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(nullptr, 1, &rx, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 0, &rx, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 1, nullptr, 1);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));

  st = dev._i2cWriteReadRaw(&byte, 1, &rx, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM), static_cast<uint8_t>(st.code));
}

void test_signed_raw_and_voltage_helpers_are_preserved() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.gain = Gain::FSR_2_048V;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.regs[cmd::REG_CONVERSION] = 0x8000;
  bus.nowMs = 4000;
  Status st = dev.startConversion();
  TEST_ASSERT_TRUE(st.inProgress());
  bus.nowMs += dev.getConversionTimeMs();
  dev.tick(bus.nowMs);
  int16_t raw = 0;
  st = dev.readRaw(raw);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(-32768, raw);

  TEST_ASSERT_FLOAT_WITHIN(0.000001f, -2.048f, dev.rawToVoltage(raw));
  TEST_ASSERT_FLOAT_WITHIN(0.000001f, 62.5e-6f, dev.getLsbVoltage());

  FakeBus continuousBus;
  ADS1115::ADS1115 continuousDev;
  Config continuousCfg = makeConfig(continuousBus);
  continuousCfg.mode = Mode::CONTINUOUS;
  continuousCfg.gain = Gain::FSR_2_048V;
  TEST_ASSERT_TRUE(continuousDev.begin(continuousCfg).ok());
  continuousBus.regs[cmd::REG_CONVERSION] = 0x7FFF;
  float volts = 0.0f;
  st = continuousDev.readVoltage(volts);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FLOAT_WITHIN(0.000001f, 2.0479375f, volts);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_ok);
  RUN_TEST(test_status_error);
  RUN_TEST(test_status_in_progress);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_begin_rejects_missing_now_ms);
  RUN_TEST(test_begin_success_sets_ready_and_counters);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);
  RUN_TEST(test_tracked_transport_errors_propagate_and_drive_health_state);
  RUN_TEST(test_single_shot_timing_wraparound_reaches_ready);
  RUN_TEST(test_read_blocking_timeout_is_bounded_and_clears_conversion_state);
  RUN_TEST(test_begin_partial_apply_failure_leaves_partial_register_state);
  RUN_TEST(test_recover_partial_apply_failure_preserves_mismatch_until_retry);
  RUN_TEST(test_setter_partial_apply_mismatch_recover_clears);
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  RUN_TEST(test_signed_raw_and_voltage_helpers_are_preserved);
  return UNITY_END();
}
