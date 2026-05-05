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
  Status writeStatus = Status::Ok();
  Status readStatus = Status::Ok();
  Status failWriteStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -1);
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
  uint8_t lastWriteReg = 0xFF;
  uint16_t lastWriteValue = 0;
  bool gpioLevel = true;
};

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
  if (txData == nullptr || txLen == 0 || (rxLen > 0 && rxData == nullptr)) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C buffers");
  }
  if (txData[0] >= 4) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake register");
  }
  const uint16_t value = bus->reg[txData[0]];
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

void test_offline_latches_normal_read_without_i2c_until_recover() {
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY), static_cast<uint8_t>(st.code));
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Driver is offline; call recover()", st.msg);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
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
  RUN_TEST(test_config_defaults);
  RUN_TEST(test_get_settings_snapshot);
  RUN_TEST(test_begin_rejects_missing_callbacks);
  RUN_TEST(test_begin_success_sets_ready_and_counters);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_recover_failure_updates_health);
  RUN_TEST(test_recover_success_returns_ready);
  RUN_TEST(test_recover_reaches_offline_when_threshold_is_one);
  RUN_TEST(test_offline_latches_normal_read_without_i2c_until_recover);
  RUN_TEST(test_failed_recover_from_offline_preserves_latch_after_partial_success);
  RUN_TEST(test_single_shot_timing_wraparound_reaches_ready);
  RUN_TEST(test_read_conversion_ready_propagates_i2c_failure);
  RUN_TEST(test_conversion_ready_convenience_returns_false_on_failure);
  RUN_TEST(test_read_raw_propagates_ready_poll_failure);
  RUN_TEST(test_read_raw_reconstructs_signed_conversion_register);
  RUN_TEST(test_continuous_readiness_waits_for_data_rate_interval);
  RUN_TEST(test_alert_ready_pin_path_does_not_poll_config_register);
  RUN_TEST(test_config_setters_write_expected_config_bits);
  RUN_TEST(test_threshold_writes_commit_cache_after_both_registers_succeed);
  RUN_TEST(test_set_gain_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_set_thresholds_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_comparator_setter_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_enable_conversion_ready_pin_rolls_back_cache_on_write_failure);
  RUN_TEST(test_invalid_raw_register_is_rejected_without_bus_access);
  RUN_TEST(test_read_blocking_times_out_when_injected_clock_stalls);
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  RUN_TEST(test_register_access_after_end_does_not_touch_bus);
  RUN_TEST(test_end_while_offline_does_not_touch_bus);
  return UNITY_END();
}
