/// @file test_basic.cpp
/// @brief Native contract tests for ADS1115 lifecycle and health behavior.

#include <unity.h>
#include <type_traits>

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
  Status writeStatus = Status::Ok();
  Status readStatus = Status::Ok();
  Status failWriteStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -1);
  Status failReadStatus = Status::Error(Err::I2C_ERROR, "forced read failure", -1);
  uint16_t reg[4] = {
    cmd::CONVERSION_DEFAULT,
    cmd::CONFIG_DEFAULT,
    cmd::LO_THRESH_DEFAULT,
    cmd::HI_THRESH_DEFAULT
  };
  uint32_t nowMs = 1234;
  uint32_t writeCalls = 0;
  uint32_t readCalls = 0;
  uint32_t failWriteOnCall = 0;
  uint32_t failReadOnCall = 0;
  uint16_t configReadOrMask = 0;
  uint16_t configReadXorMask = 0;
  uint16_t readOrMask[4] = {0, 0, 0, 0};
  uint16_t readXorMask[4] = {0, 0, 0, 0};
  uint8_t lastWriteReg = 0xFF;
  uint16_t lastWriteValue = 0;
  bool gpioLevel = true;
};

static_assert(!std::is_copy_constructible<ADS1115::ADS1115>::value,
              "ADS1115 driver instances must not be copy constructible");
static_assert(!std::is_copy_assignable<ADS1115::ADS1115>::value,
              "ADS1115 driver instances must not be copy assignable");
static_assert(!std::is_move_constructible<ADS1115::ADS1115>::value,
              "ADS1115 driver instances must not be move constructible");
static_assert(!std::is_move_assignable<ADS1115::ADS1115>::value,
              "ADS1115 driver instances must not be move assignable");

void resetIoCounters(FakeBus& bus) {
  bus.writeCalls = 0;
  bus.readCalls = 0;
  bus.failWriteOnCall = 0;
  bus.failReadOnCall = 0;
}

Status fakeWrite(uint8_t, const uint8_t* data, size_t len, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  if (data == nullptr || len != 3) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C write");
  }
  if (bus->failWriteOnCall != 0 && bus->writeCalls == bus->failWriteOnCall) {
    return bus->failWriteStatus;
  }
  if (!bus->writeStatus.ok()) {
    return bus->writeStatus;
  }
  bus->lastWriteReg = data[0];
  bus->lastWriteValue = (static_cast<uint16_t>(data[1]) << 8) | data[2];
  if (bus->lastWriteReg < 4) {
    bus->reg[bus->lastWriteReg] = bus->lastWriteValue;
  }
  return bus->writeStatus;
}

Status fakeWriteRead(uint8_t, const uint8_t* txData, size_t txLen, uint8_t* rxData,
                     size_t rxLen, uint32_t, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->readCalls++;
  if (!bus->readStatus.ok()) {
    return bus->readStatus;
  }
  if (bus->failReadOnCall != 0 && bus->readCalls == bus->failReadOnCall) {
    return bus->failReadStatus;
  }
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C buffers");
  }
  if (txData[0] >= 4) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake register");
  }
  uint8_t reg = txData[0];
  uint16_t value = bus->reg[reg];
  value = static_cast<uint16_t>((value | bus->readOrMask[reg]) ^ bus->readXorMask[reg]);
  if (txData[0] == cmd::REG_CONFIG) {
    value = static_cast<uint16_t>((value | bus->configReadOrMask) ^ bus->configReadXorMask);
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

bool fakeGpioRead(int, void* user) {
  return static_cast<FakeBus*>(user)->gpioLevel;
}

void fakeYield(void*) {}

Config makeConfig(FakeBus& bus) {
  Config cfg;
  cfg.i2cWrite = fakeWrite;
  cfg.i2cWriteRead = fakeWriteRead;
  cfg.i2cUser = &bus;
  cfg.nowMs = fakeNowMs;
  cfg.timeUser = &bus;
  cfg.gpioUser = &bus;
  cfg.offlineThreshold = 3;
  cfg.i2cTimeoutMs = 10;
  return cfg;
}

void assertDirtyDiagnostic(const ADS1115::ADS1115& dev, Err expectedCode,
                           int32_t expectedDetail) {
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedCode),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_INT32(expectedDetail, dev.hardwareConfigDirtyError().detail);

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.hardwareConfigDirty);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expectedCode),
                          static_cast<uint8_t>(snap.hardwareConfigDirtyError.code));
  TEST_ASSERT_EQUAL_INT32(expectedDetail, snap.hardwareConfigDirtyError.detail);
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

void test_status_taxonomy_additions_are_append_only() {
  TEST_ASSERT_EQUAL_UINT8(14u, static_cast<uint8_t>(Err::OFFLINE));
  TEST_ASSERT_EQUAL_UINT8(15u, static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION));
  TEST_ASSERT_EQUAL_UINT8(16u, static_cast<uint8_t>(Err::READBACK_MISMATCH));
  TEST_ASSERT_EQUAL_UINT8(17u, static_cast<uint8_t>(Err::HARDWARE_CONFIG_DIRTY));
}

void test_config_defaults() {
  Config cfg;
  TEST_ASSERT_NULL(cfg.i2cWrite);
  TEST_ASSERT_NULL(cfg.i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x48, cfg.i2cAddress);
  TEST_ASSERT_EQUAL_UINT16(50, cfg.i2cTimeoutMs);
  TEST_ASSERT_FALSE(cfg.strictInitVerify);
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
  TEST_ASSERT_FALSE(snap.strictInitVerify);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_TRUE(snap.hasGpioReadHook);
  TEST_ASSERT_TRUE(snap.hasCooperativeYieldHook);
  TEST_ASSERT_FALSE(snap.hardwareConfigDirty);
  TEST_ASSERT_TRUE(snap.hardwareConfigDirtyError.ok());
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
  TEST_ASSERT_TRUE(snap.conversionStarted);
  TEST_ASSERT_FALSE(snap.conversionReady);
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, snap.conversionStartMs);
  TEST_ASSERT_EQUAL_INT16(0, snap.lastRawValue);
}

void test_begin_rejects_missing_callbacks() {
  ADS1115::ADS1115 dev;
  Config cfg;
  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_invalid_begin_resets_runtime_and_default_config() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config good = makeConfig(bus);
  good.i2cAddress = 0x4B;
  TEST_ASSERT_TRUE(dev.begin(good).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced recover timeout", -9);
  (void)dev.recover();
  TEST_ASSERT_GREATER_THAN_UINT32(0u, dev.totalFailures());

  Config bad = makeConfig(bus);
  bad.i2cTimeoutMs = 0;
  Status st = dev.begin(bad);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_NULL(dev.getConfig().i2cWrite);
  TEST_ASSERT_NULL(dev.getConfig().i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x48, dev.getConfig().i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(5u, dev.getConfig().offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastErrorMs());
}

void test_failed_begin_probe_resets_cached_config() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x4B;
  cfg.mode = Mode::CONTINUOUS;
  cfg.offlineThreshold = 1;
  bus.readStatus = Status::Error(Err::TIMEOUT, "forced begin timeout", -10);

  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_NULL(dev.getConfig().i2cWrite);
  TEST_ASSERT_NULL(dev.getConfig().i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x48, dev.getConfig().i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SINGLE_SHOT),
                          static_cast<uint8_t>(dev.getConfig().mode));
  TEST_ASSERT_EQUAL_UINT8(5u, dev.getConfig().offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
}

void test_begin_normalizes_offline_threshold_on_stored_copy() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 0;

  Status st = dev.begin(cfg);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, cfg.offlineThreshold);
  TEST_ASSERT_EQUAL_UINT8(1u, dev.getConfig().offlineThreshold);
}

void test_begin_success_sets_ready_and_counters() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Status st = dev.begin(makeConfig(bus));
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.isOnline());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastOkMs());
}

void test_begin_strict_readback_success_masks_config_os_bit() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  bus.configReadOrMask = cmd::MASK_OS;

  Status st = dev.begin(cfg);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.isInitialized());
  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.strictInitVerify);
  TEST_ASSERT_FALSE(snap.hardwareConfigDirty);
}

void test_begin_strict_readback_mismatch_fails_without_initializing_and_preserves_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  bus.configReadXorMask = cmd::MASK_DR;

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(bus.reg[cmd::REG_CONFIG] ^ cmd::MASK_DR),
                          dev.hardwareConfigDirtyError().detail);
}

void test_begin_strict_low_threshold_readback_mismatch_reports_observed_detail() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  bus.readXorMask[cmd::REG_LO_THRESH] = 0x0001;

  Status st = dev.begin(cfg);

  const int32_t observed = static_cast<int32_t>(bus.reg[cmd::REG_LO_THRESH] ^ 0x0001);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(observed, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  assertDirtyDiagnostic(dev, Err::READBACK_MISMATCH, observed);
}

void test_begin_strict_high_threshold_readback_mismatch_reports_observed_detail() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  bus.readXorMask[cmd::REG_HI_THRESH] = 0x0001;

  Status st = dev.begin(cfg);

  const int32_t observed = static_cast<int32_t>(bus.reg[cmd::REG_HI_THRESH] ^ 0x0001);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(observed, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  assertDirtyDiagnostic(dev, Err::READBACK_MISMATCH, observed);
}

void test_begin_failure_after_first_apply_write_preserves_dirty_diagnostic() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "second begin write bus", -52);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-52, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT16(cmd::LO_THRESH_DEFAULT, bus.reg[cmd::REG_LO_THRESH]);
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -52);
}

void test_begin_failure_after_second_apply_write_preserves_dirty_diagnostic() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  bus.failWriteOnCall = 3;
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "third begin write failure", -53);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-53, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT16(cmd::LO_THRESH_DEFAULT, bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(cmd::HI_THRESH_DEFAULT, bus.reg[cmd::REG_HI_THRESH]);
  assertDirtyDiagnostic(dev, Err::I2C_ERROR, -53);
}

void test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  bus.failReadOnCall = 4;
  bus.failReadStatus = Status::Error(Err::I2C_TIMEOUT, "strict config read timeout", -54);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-54, st.detail);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -54);
}

void test_successful_begin_clears_prior_failed_begin_dirty_diagnostic() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "second begin write bus", -55);

  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -55);

  resetIoCounters(bus);
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "unused write failure", -1);
  st = dev.begin(cfg);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());
}

void test_failed_begin_dirty_survives_later_probe_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "second begin write bus", -56);

  Status st = dev.begin(cfg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -56);

  resetIoCounters(bus);
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "retry probe timeout", -57);
  st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -56);
}

void test_recover_strict_readback_success_clears_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  TEST_ASSERT_FALSE(dev.recover().ok());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());

  resetIoCounters(bus);
  bus.configReadOrMask = cmd::MASK_OS;
  Status st = dev.recover();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_recover_strict_readback_mismatch_keeps_dirty_and_preserves_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  TEST_ASSERT_FALSE(dev.recover().ok());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());

  resetIoCounters(bus);
  bus.configReadXorMask = cmd::MASK_MODE;
  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(bus.reg[cmd::REG_CONFIG] ^ cmd::MASK_MODE),
                          dev.hardwareConfigDirtyError().detail);
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(beforeConsecutive, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeState),
                          static_cast<uint8_t>(dev.state()));
}

void test_probe_maps_i2c_nack_addr_to_device_not_found() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  bus.readStatus = Status::Error(Err::I2C_NACK_ADDR, "address nack", -20);
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-20, st.detail);
}

void test_probe_preserves_distinct_i2c_errors() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Err errors[] = {Err::I2C_TIMEOUT, Err::TIMEOUT, Err::I2C_BUS,
                        Err::I2C_NACK_DATA, Err::I2C_ERROR};
  for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
    bus.readStatus = Status::Error(errors[i], "probe failure", static_cast<int32_t>(-30 - i));
    Status st = dev.probe();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(errors[i]), static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(static_cast<int32_t>(-30 - i), st.detail);
  }
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
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

void test_offline_latches_normal_read_returns_offline_without_i2c_until_recover() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -11);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.recover().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t readsBefore = bus.readCalls;
  uint16_t value = 0;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  TEST_ASSERT_TRUE(dev.recover().ok());
  TEST_ASSERT_GREATER_THAN_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
}

void test_failed_recover_from_offline_preserves_latch_after_partial_success() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 3;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -12);
  uint16_t value = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    Status st = dev.readRegister16(cmd::REG_CONFIG, value);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                            static_cast<uint8_t>(st.code));
  }
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(3u, dev.consecutiveFailures());

  bus.readStatus = Status::Ok();
  bus.failWriteOnCall = bus.writeCalls + 1u;
  Status st = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.consecutiveFailures() >= 3u);
  TEST_ASSERT_FALSE(dev._allowOfflineI2c);

  bus.failWriteOnCall = 0;
  const uint32_t readsBefore = bus.readCalls;
  st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

void test_start_conversion_in_continuous_mode_returns_unsupported_operation() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  const uint32_t writesBefore = bus.writeCalls;

  Status st = dev.startConversion();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);

  st = dev.startConversion(Mux::AIN1_GND);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
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

void test_single_shot_raw_read_consumes_ready_before_voltage_read() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x1000;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.startConversion();
  TEST_ASSERT_TRUE(st.inProgress());
  bus.nowMs += dev.getConversionTimeMs();
  dev.tick(bus.nowMs);
  TEST_ASSERT_TRUE(dev.conversionReady());

  int16_t raw = 0;
  st = dev.readRaw(raw);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(0x1000, raw);

  float volts = 0.0f;
  st = dev.readVoltage(volts);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CONVERSION_NOT_READY),
                          static_cast<uint8_t>(st.code));
}

void test_read_conversion_ready_propagates_i2c_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced timeout", -2);
  bool ready = true;
  Status st = dev.readConversionReady(ready);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(ready);
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
}

void test_conversion_ready_convenience_returns_false_on_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced timeout", -3);

  TEST_ASSERT_FALSE(dev.conversionReady());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
}

void test_read_raw_propagates_ready_poll_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "forced timeout", -4);
  int16_t raw = 0;
  Status st = dev.readRaw(raw);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
}

void test_read_raw_reconstructs_signed_conversion_register() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0xFFFE;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  int16_t raw = 0;
  Status st = dev.readRaw(raw);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(-2, raw);
  TEST_ASSERT_EQUAL_INT16(-2, dev._lastRawValue);
}

void test_continuous_readiness_waits_for_data_rate_interval() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.dataRate = DataRate::SPS_128;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bool ready = true;
  Status st = dev.readConversionReady(ready);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(ready);

  bus.nowMs += dev.getConversionTimeMs();
  st = dev.readConversionReady(ready);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(ready);
}

void test_continuous_read_raw_returns_latest_without_fresh_wait() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x1234;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.dataRate = DataRate::SPS_8;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bool ready = true;
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_FALSE(ready);

  int16_t raw = 0;
  Status st = dev.readRaw(raw);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(0x1234, raw);
}

void test_alert_ready_pin_path_does_not_poll_config_register() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.alertRdyPin = 17;
  cfg.gpioRead = fakeGpioRead;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.enableConversionReadyPin().ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  const uint32_t readsBefore = bus.readCalls;
  bus.gpioLevel = false;  // Default comparator polarity is active-low.
  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "should not poll config", -5);

  bool ready = false;
  Status st = dev.readConversionReady(ready);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(ready);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

void test_config_setters_write_expected_config_bits() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  TEST_ASSERT_TRUE(dev.setMux(Mux::AIN2_AIN3).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Mux::AIN2_AIN3),
                           (bus.reg[cmd::REG_CONFIG] & cmd::MASK_MUX) >> cmd::BIT_MUX);

  TEST_ASSERT_TRUE(dev.setGain(Gain::FSR_0_512V).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Gain::FSR_0_512V),
                           (bus.reg[cmd::REG_CONFIG] & cmd::MASK_PGA) >> cmd::BIT_PGA);

  TEST_ASSERT_TRUE(dev.setDataRate(DataRate::SPS_860).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(DataRate::SPS_860),
                           (bus.reg[cmd::REG_CONFIG] & cmd::MASK_DR) >> cmd::BIT_DR);

  TEST_ASSERT_TRUE(dev.setMode(Mode::CONTINUOUS).ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Mode::CONTINUOUS),
                           (bus.reg[cmd::REG_CONFIG] & cmd::MASK_MODE) >> cmd::BIT_MODE);
  TEST_ASSERT_TRUE(dev._conversionStarted);
}

void test_write_config_accepts_datasheet_pga_aliases_for_0_256v() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  uint16_t config = static_cast<uint16_t>((cmd::CONFIG_DEFAULT & ~cmd::MASK_PGA) |
                                          cmd::PGA_0_256V_ALT1);
  Status st = dev.writeConfig(config);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(6u, (bus.reg[cmd::REG_CONFIG] & cmd::MASK_PGA) >> cmd::BIT_PGA);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_256V),
                          static_cast<uint8_t>(dev.getGain()));

  config = static_cast<uint16_t>((cmd::CONFIG_DEFAULT & ~cmd::MASK_PGA) |
                                 cmd::PGA_0_256V_ALT2);
  st = dev.writeConfig(config);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(7u, (bus.reg[cmd::REG_CONFIG] & cmd::MASK_PGA) >> cmd::BIT_PGA);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_256V),
                          static_cast<uint8_t>(dev.getGain()));
}

void test_threshold_writes_commit_cache_after_both_registers_succeed() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setThresholds(-100, 250);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(-100), bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(250), bus.reg[cmd::REG_HI_THRESH]);
  TEST_ASSERT_EQUAL_INT16(-100, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(250, dev.getConfig().compThresholdHigh);
}

void test_set_gain_does_not_commit_cache_on_write_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Gain oldGain = dev.getGain();

  bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -6);
  Status st = dev.setGain(Gain::FSR_0_256V);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldGain),
                          static_cast<uint8_t>(dev.getGain()));
}

void test_set_thresholds_does_not_commit_cache_on_write_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const int16_t oldLow = dev.getConfig().compThresholdLow;
  const int16_t oldHigh = dev.getConfig().compThresholdHigh;

  bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -7);
  Status st = dev.setThresholds(-100, 250);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT16(oldLow, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(oldHigh, dev.getConfig().compThresholdHigh);
}

void test_comparator_setter_does_not_commit_cache_on_write_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const ComparatorQueue oldQueue = dev.getComparatorQueue();

  bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -8);
  Status st = dev.setComparatorQueue(ComparatorQueue::ASSERT_1);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldQueue),
                          static_cast<uint8_t>(dev.getComparatorQueue()));
}

void test_enable_conversion_ready_pin_rolls_back_cache_on_write_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Config oldConfig = dev.getConfig();

  bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -9);
  Status st = dev.enableConversionReadyPin();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT16(oldConfig.compThresholdLow, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(oldConfig.compThresholdHigh, dev.getConfig().compThresholdHigh);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldConfig.compQueue),
                          static_cast<uint8_t>(dev.getConfig().compQueue));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldConfig.compMode),
                          static_cast<uint8_t>(dev.getConfig().compMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldConfig.compLatch),
                          static_cast<uint8_t>(dev.getConfig().compLatch));
}

void test_apply_config_failure_on_first_write_preserves_error_not_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "first write timeout", -41);

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-41, st.detail);
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_apply_config_failure_on_second_write_marks_hardware_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "second write bus", -42);

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_INT32(-42, dev.hardwareConfigDirtyError().detail);
}

void test_apply_config_failure_on_third_write_marks_hardware_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 3;
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "third write failure", -43);

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.hardwareConfigDirty);
  TEST_ASSERT_EQUAL_INT32(-43, snap.hardwareConfigDirtyError.detail);
}

void test_set_thresholds_second_write_failure_marks_hardware_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "high threshold timeout", -44);

  Status st = dev.setThresholds(-123, 456);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(-123), bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_FALSE(bus.reg[cmd::REG_HI_THRESH] == static_cast<uint16_t>(456));
}

void test_enable_conversion_ready_pin_partial_failure_marks_hardware_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "partial ready pin failure", -45);

  Status st = dev.enableConversionReadyPin();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_INT32(-45, dev.hardwareConfigDirtyError().detail);
}

void test_recover_success_clears_hardware_dirty_after_full_resync() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  TEST_ASSERT_FALSE(dev.recover().ok());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());

  resetIoCounters(bus);
  Status st = dev.recover();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());
}

void test_successful_config_only_setter_does_not_clear_prior_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  TEST_ASSERT_FALSE(dev.setThresholds(-11, 22).ok());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());

  resetIoCounters(bus);
  Status st = dev.setGain(Gain::FSR_0_512V);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
}

void test_invalid_raw_register_is_rejected_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;

  uint16_t value = 0;
  Status st = dev.readRegister16(4, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.writeRegister16(4, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_write_conversion_register_is_rejected_as_read_only() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint32_t writesBefore = bus.writeCalls;
  const uint16_t conversionBefore = bus.reg[cmd::REG_CONVERSION];

  Status st = dev.writeRegister16(cmd::REG_CONVERSION, 0x1234);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Register is read-only", st.msg);
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT16(conversionBefore, bus.reg[cmd::REG_CONVERSION]);
}

void test_raw_config_write_marks_hardware_config_dirty_without_cache_commit() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Config cached = dev.getConfig();
  uint16_t rawConfig = cmd::CONFIG_DEFAULT;
  rawConfig &= static_cast<uint16_t>(~cmd::MASK_MUX);
  rawConfig |= (static_cast<uint16_t>(Mux::AIN2_GND) << cmd::BIT_MUX) & cmd::MASK_MUX;
  rawConfig &= static_cast<uint16_t>(~cmd::MASK_DR);
  rawConfig |= (static_cast<uint16_t>(DataRate::SPS_860) << cmd::BIT_DR) & cmd::MASK_DR;

  Status st = dev.writeRegister16(cmd::REG_CONFIG, rawConfig);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(rawConfig, bus.reg[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cached.mux),
                          static_cast<uint8_t>(dev.getConfig().mux));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cached.dataRate),
                          static_cast<uint8_t>(dev.getConfig().dataRate));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cached.mode),
                          static_cast<uint8_t>(dev.getConfig().mode));
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
}

void test_raw_low_threshold_write_marks_hardware_config_dirty_without_cache_commit() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const int16_t cachedLow = dev.getConfig().compThresholdLow;
  const int16_t cachedHigh = dev.getConfig().compThresholdHigh;
  const uint16_t rawLow = static_cast<uint16_t>(-321);

  Status st = dev.writeRegister16(cmd::REG_LO_THRESH, rawLow);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(rawLow, bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_INT16(cachedLow, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(cachedHigh, dev.getConfig().compThresholdHigh);
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_LO_THRESH);
}

void test_raw_high_threshold_write_marks_hardware_config_dirty_without_cache_commit() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const int16_t cachedLow = dev.getConfig().compThresholdLow;
  const int16_t cachedHigh = dev.getConfig().compThresholdHigh;
  const uint16_t rawHigh = static_cast<uint16_t>(1234);

  Status st = dev.writeRegister16(cmd::REG_HI_THRESH, rawHigh);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(rawHigh, bus.reg[cmd::REG_HI_THRESH]);
  TEST_ASSERT_EQUAL_INT16(cachedLow, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(cachedHigh, dev.getConfig().compThresholdHigh);
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_HI_THRESH);
}

void test_write_register_alias_marks_hardware_config_dirty_without_cache_commit() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const int16_t cachedLow = dev.getConfig().compThresholdLow;
  const uint16_t rawLow = static_cast<uint16_t>(-99);

  Status st = dev.writeRegister(cmd::REG_LO_THRESH, rawLow);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(rawLow, bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_INT16(cachedLow, dev.getConfig().compThresholdLow);
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_LO_THRESH);
}

void test_failed_raw_write_marks_hardware_config_dirty_with_transport_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint16_t configBefore = bus.reg[cmd::REG_CONFIG];
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "raw write timeout", -60);

  Status st = dev.writeRegister16(cmd::REG_CONFIG,
                                  static_cast<uint16_t>(configBefore ^ cmd::MASK_DR));

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-60, st.detail);
  TEST_ASSERT_EQUAL_UINT16(configBefore, bus.reg[cmd::REG_CONFIG]);
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -60);
}

void test_raw_write_offline_returns_offline_without_dirty_or_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  bus.readStatus = Status::Error(Err::TIMEOUT, "forced offline", -61);
  TEST_ASSERT_FALSE(dev.recover().ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t writesBefore = bus.writeCalls;
  const uint16_t rawConfig = static_cast<uint16_t>(cmd::CONFIG_DEFAULT ^ cmd::MASK_DR);
  Status st = dev.writeRegister16(cmd::REG_CONFIG, rawConfig);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());
}

void test_recover_success_clears_raw_register_dirty_after_full_resync() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint16_t cachedConfig = bus.reg[cmd::REG_CONFIG];
  const uint16_t rawConfig = static_cast<uint16_t>(cachedConfig ^ cmd::MASK_DR);
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, rawConfig).ok());
  TEST_ASSERT_EQUAL_UINT16(rawConfig, bus.reg[cmd::REG_CONFIG]);
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);

  resetIoCounters(bus);
  Status st = dev.recover();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(cachedConfig, bus.reg[cmd::REG_CONFIG]);
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirtyError().ok());
}

void test_recover_raw_dirty_requires_verified_readback_before_clear() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint16_t rawConfig = static_cast<uint16_t>(cmd::CONFIG_DEFAULT ^ cmd::MASK_DR);
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, rawConfig).ok());
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  resetIoCounters(bus);
  bus.readXorMask[cmd::REG_CONFIG] = cmd::MASK_DR;

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
}

void test_failed_recover_probe_preserves_raw_register_dirty_reason() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint16_t rawConfig = static_cast<uint16_t>(cmd::CONFIG_DEFAULT ^ cmd::MASK_DR);
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, rawConfig).ok());
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  resetIoCounters(bus);
  bus.failReadOnCall = 1;
  bus.failReadStatus = Status::Error(Err::I2C_BUS, "recover probe failed", -61);

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-61, st.detail);
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
}

void test_failed_recover_partial_apply_keeps_raw_register_dirty_visible() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const uint16_t rawHigh = static_cast<uint16_t>(777);
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_HI_THRESH, rawHigh).ok());
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_HI_THRESH);
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "recover high threshold failed", -62);

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-62, st.detail);
  assertDirtyDiagnostic(dev, Err::I2C_ERROR, -62);
}

void test_read_blocking_times_out_when_injected_clock_stalls() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
}

void test_read_blocking_requires_now_ms_without_starting_conversion() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  const uint32_t writesBefore = bus.writeCalls;

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
}

void test_read_blocking_voltage_requires_now_ms_without_starting_conversion() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  const uint32_t writesBefore = bus.writeCalls;

  float volts = 0.0f;
  Status st = dev.readBlockingVoltage(volts, 200);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
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

void test_register_access_after_end_does_not_touch_bus() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t writesAfterBegin = bus.writeCalls;
  const uint32_t readsAfterBegin = bus.readCalls;

  dev.end();
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin + 1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);

  uint16_t value = 0;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);

  st = dev.writeRegister16(cmd::REG_CONFIG, cmd::CONFIG_DEFAULT);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin + 1u, bus.writeCalls);
}

void test_shutdown_success_writes_single_shot_mode_and_keeps_initialized() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.shutdown();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SINGLE_SHOT),
                          static_cast<uint8_t>(dev.getMode()));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(Mode::SINGLE_SHOT),
                           (bus.reg[cmd::REG_CONFIG] & cmd::MASK_MODE) >> cmd::BIT_MODE);
  TEST_ASSERT_FALSE(dev._conversionStarted);
}

void test_shutdown_returns_original_transport_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "shutdown timeout", -61);

  Status st = dev.shutdown();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-61, st.detail);
  TEST_ASSERT_TRUE(dev.isInitialized());
}

void test_shutdown_offline_returns_offline_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "offline", -62);
  TEST_ASSERT_FALSE(dev.recover().ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  const uint32_t writesBefore = bus.writeCalls;

  Status st = dev.shutdown();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::OFFLINE), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
}

void test_end_ignores_shutdown_write_failure_and_uninitializes() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "end timeout", -63);

  dev.end();

  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_end_while_offline_does_not_touch_bus() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -13);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.recover().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  const uint32_t writesBefore = bus.writeCalls;
  dev.end();
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_status_ok);
  RUN_TEST(test_status_error);
  RUN_TEST(test_status_in_progress);
  RUN_TEST(test_status_taxonomy_additions_are_append_only);
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_invalid_begin_resets_runtime_and_default_config);
  RUN_TEST(test_failed_begin_probe_resets_cached_config);
  RUN_TEST(test_begin_normalizes_offline_threshold_on_stored_copy);
  RUN_TEST(test_begin_success_sets_ready_and_counters);
  RUN_TEST(test_begin_strict_readback_success_masks_config_os_bit);
  RUN_TEST(test_begin_strict_readback_mismatch_fails_without_initializing_and_preserves_dirty);
  RUN_TEST(test_begin_strict_low_threshold_readback_mismatch_reports_observed_detail);
  RUN_TEST(test_begin_strict_high_threshold_readback_mismatch_reports_observed_detail);
  RUN_TEST(test_begin_failure_after_first_apply_write_preserves_dirty_diagnostic);
  RUN_TEST(test_begin_failure_after_second_apply_write_preserves_dirty_diagnostic);
  RUN_TEST(test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic);
  RUN_TEST(test_successful_begin_clears_prior_failed_begin_dirty_diagnostic);
  RUN_TEST(test_failed_begin_dirty_survives_later_probe_failure);
  RUN_TEST(test_recover_strict_readback_success_clears_dirty);
  RUN_TEST(test_recover_strict_readback_mismatch_keeps_dirty_and_preserves_error);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_maps_i2c_nack_addr_to_device_not_found);
  RUN_TEST(test_probe_preserves_distinct_i2c_errors);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);
  RUN_TEST(test_offline_latches_normal_read_returns_offline_without_i2c_until_recover);
  RUN_TEST(test_failed_recover_from_offline_preserves_latch_after_partial_success);
  RUN_TEST(test_start_conversion_in_continuous_mode_returns_unsupported_operation);
  RUN_TEST(test_single_shot_timing_wraparound_reaches_ready);
  RUN_TEST(test_single_shot_raw_read_consumes_ready_before_voltage_read);
  RUN_TEST(test_read_conversion_ready_propagates_i2c_failure);
  RUN_TEST(test_conversion_ready_convenience_returns_false_on_failure);
  RUN_TEST(test_read_raw_propagates_ready_poll_failure);
  RUN_TEST(test_read_raw_reconstructs_signed_conversion_register);
  RUN_TEST(test_continuous_readiness_waits_for_data_rate_interval);
  RUN_TEST(test_continuous_read_raw_returns_latest_without_fresh_wait);
  RUN_TEST(test_alert_ready_pin_path_does_not_poll_config_register);
  RUN_TEST(test_config_setters_write_expected_config_bits);
  RUN_TEST(test_write_config_accepts_datasheet_pga_aliases_for_0_256v);
  RUN_TEST(test_threshold_writes_commit_cache_after_both_registers_succeed);
  RUN_TEST(test_set_gain_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_set_thresholds_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_comparator_setter_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_enable_conversion_ready_pin_rolls_back_cache_on_write_failure);
  RUN_TEST(test_apply_config_failure_on_first_write_preserves_error_not_dirty);
  RUN_TEST(test_apply_config_failure_on_second_write_marks_hardware_dirty);
  RUN_TEST(test_apply_config_failure_on_third_write_marks_hardware_dirty);
  RUN_TEST(test_set_thresholds_second_write_failure_marks_hardware_dirty);
  RUN_TEST(test_enable_conversion_ready_pin_partial_failure_marks_hardware_dirty);
  RUN_TEST(test_recover_success_clears_hardware_dirty_after_full_resync);
  RUN_TEST(test_successful_config_only_setter_does_not_clear_prior_dirty);
  RUN_TEST(test_invalid_raw_register_is_rejected_without_bus_access);
  RUN_TEST(test_write_conversion_register_is_rejected_as_read_only);
  RUN_TEST(test_raw_config_write_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_raw_low_threshold_write_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_raw_high_threshold_write_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_write_register_alias_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_failed_raw_write_marks_hardware_config_dirty_with_transport_error);
  RUN_TEST(test_raw_write_offline_returns_offline_without_dirty_or_bus_access);
  RUN_TEST(test_recover_success_clears_raw_register_dirty_after_full_resync);
  RUN_TEST(test_recover_raw_dirty_requires_verified_readback_before_clear);
  RUN_TEST(test_failed_recover_probe_preserves_raw_register_dirty_reason);
  RUN_TEST(test_failed_recover_partial_apply_keeps_raw_register_dirty_visible);
  RUN_TEST(test_read_blocking_times_out_when_injected_clock_stalls);
  RUN_TEST(test_read_blocking_requires_now_ms_without_starting_conversion);
  RUN_TEST(test_read_blocking_voltage_requires_now_ms_without_starting_conversion);
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  RUN_TEST(test_register_access_after_end_does_not_touch_bus);
  RUN_TEST(test_shutdown_success_writes_single_shot_mode_and_keeps_initialized);
  RUN_TEST(test_shutdown_returns_original_transport_error);
  RUN_TEST(test_shutdown_offline_returns_offline_without_bus_access);
  RUN_TEST(test_end_ignores_shutdown_write_failure_and_uninitializes);
  RUN_TEST(test_end_while_offline_does_not_touch_bus);
  return UNITY_END();
}
