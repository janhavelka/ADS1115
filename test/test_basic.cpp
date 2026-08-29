/// @file test_basic.cpp
/// @brief Native contract tests for ADS1115 lifecycle and health behavior.

#include <unity.h>
#include <type_traits>

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
  uint32_t yieldCalls = 0;
  uint32_t failWriteOnCall = 0;
  uint32_t failReadOnCall = 0;
  bool applyFailedWrite = false;
  uint32_t writeAdvanceMs = 0;
  uint32_t lastWriteTimeoutMs = 0;
  uint32_t lastReadTimeoutMs = 0;
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
static_assert(std::is_trivially_copyable<SampleResult>::value,
              "Owner-safe samples must be trivially copyable");
static_assert(std::is_trivially_copyable<OperationResult>::value,
              "Owner-safe operation results must be trivially copyable");
static_assert(sizeof(SampleResult) <= 32U,
              "Owner-safe sample must remain fixed and compact");
static_assert(sizeof(OperationResult) <= 128U,
              "Owner-safe terminal result must remain bounded");

void resetIoCounters(FakeBus& bus) {
  bus.writeCalls = 0;
  bus.readCalls = 0;
  bus.yieldCalls = 0;
  bus.failWriteOnCall = 0;
  bus.failReadOnCall = 0;
  bus.applyFailedWrite = false;
  bus.writeAdvanceMs = 0;
  bus.lastWriteTimeoutMs = 0;
  bus.lastReadTimeoutMs = 0;
}

Status fakeWrite(uint8_t, const uint8_t* data, size_t len, uint32_t timeoutMs,
                 void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->writeCalls++;
  bus->lastWriteTimeoutMs = timeoutMs;
  bus->nowMs += bus->writeAdvanceMs;
  if (data == nullptr || len != 3) {
    return Status::Error(Err::INVALID_PARAM, "invalid fake I2C write");
  }
  const bool failThisWrite =
      bus->failWriteOnCall != 0 && bus->writeCalls == bus->failWriteOnCall;
  if (failThisWrite && !bus->applyFailedWrite) {
    return bus->failWriteStatus;
  }
  if (!bus->writeStatus.ok() && !bus->applyFailedWrite) {
    return bus->writeStatus;
  }
  bus->lastWriteReg = data[0];
  bus->lastWriteValue = (static_cast<uint16_t>(data[1]) << 8) | data[2];
  if (bus->lastWriteReg < 4) {
    bus->reg[bus->lastWriteReg] = bus->lastWriteValue;
  }
  if (failThisWrite) {
    return bus->failWriteStatus;
  }
  if (!bus->writeStatus.ok()) {
    return bus->writeStatus;
  }
  return bus->writeStatus;
}

Status fakeWriteRead(uint8_t, const uint8_t* txData, size_t txLen, uint8_t* rxData,
                     size_t rxLen, uint32_t timeoutMs, void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->readCalls++;
  bus->lastReadTimeoutMs = timeoutMs;
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

void fakeYieldAdvanceMs(void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->yieldCalls++;
  bus->nowMs++;
}

void fakeYieldAdvanceEveryOtherCall(void* user) {
  FakeBus* bus = static_cast<FakeBus*>(user);
  bus->yieldCalls++;
  if ((bus->yieldCalls % 2U) == 0U) {
    bus->nowMs++;
  }
}

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

DriverConfig makeDriverConfig(FakeBus& bus) {
  DriverConfig cfg;
  cfg.i2cWrite = fakeWrite;
  cfg.i2cWriteRead = fakeWriteRead;
  cfg.i2cUser = &bus;
  cfg.transferTimeoutMs = 10;
  return cfg;
}

DeviceProfile makeDeviceProfile() {
  DeviceProfile profile;
  profile.i2cAddress = 0x48;
  profile.defaultMux = Mux::AIN0_GND;
  profile.defaultGain = Gain::FSR_2_048V;
  profile.dataRate = DataRate::SPS_128;
  profile.mode = Mode::SINGLE_SHOT;
  profile.comparator.use = ComparatorUse::OFF;
  profile.comparator.queue = ComparatorQueue::DISABLE;
  return profile;
}

uint32_t ownerConversionTimeMs(DataRate rate) {
  return (worstCaseConversionTimeUs(rate) + 999U) / 1000U;
}

OperationResult initializeOwnerSafe(ADS1115::ADS1115& dev, FakeBus& bus,
                                    const DeviceProfile& profile,
                                    uint32_t nowMs = 100U) {
  const uint32_t writesBeforeBind = bus.writeCalls;
  const uint32_t readsBeforeBind = bus.readCalls;
  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), profile).ok());
  TEST_ASSERT_EQUAL_UINT32(writesBeforeBind, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsBeforeBind, bus.readCalls);

  OperationToken token;
  Status st = dev.startInitialize(nowMs, nowMs + 1000U, token);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(token.valid());
  TEST_ASSERT_EQUAL_UINT32(writesBeforeBind, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsBeforeBind, bus.readCalls);

  PollResult poll;
  uint8_t polls = 0;
  do {
    const uint32_t writesBeforePoll = bus.writeCalls;
    const uint32_t readsBeforePoll = bus.readCalls;
    poll = dev.poll(nowMs, 1);
    TEST_ASSERT_TRUE(poll.instructionsUsed <= 1U);
    TEST_ASSERT_EQUAL_UINT32(
        poll.instructionsUsed,
        (bus.writeCalls - writesBeforePoll) + (bus.readCalls - readsBeforePoll));
    polls++;
  } while (!poll.done && polls < 8U);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(7U, polls);
  TEST_ASSERT_EQUAL_UINT32(writesBeforeBind + 3U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsBeforeBind + 4U, bus.readCalls);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::INITIALIZE),
                          static_cast<uint8_t>(result.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::SUCCEEDED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_FALSE(result.sampleValid);
  return result;
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

void forceOffline(ADS1115::ADS1115& dev) {
  dev._driverState = DriverState::OFFLINE;
  dev._consecutiveFailures = dev.getConfig().offlineThreshold;
}

void assertBusyStatus(const Status& st) {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Poll job active", st.msg);
}

void assertNoIoSince(const FakeBus& bus, uint32_t writesBefore, uint32_t readsBefore) {
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
}

void assertDeviceProfilesEqual(const DeviceProfile& expected,
                               const DeviceProfile& actual) {
  TEST_ASSERT_EQUAL_HEX8(expected.i2cAddress, actual.i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.defaultMux),
                          static_cast<uint8_t>(actual.defaultMux));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.defaultGain),
                          static_cast<uint8_t>(actual.defaultGain));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.dataRate),
                          static_cast<uint8_t>(actual.dataRate));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.mode),
                          static_cast<uint8_t>(actual.mode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.comparator.use),
                          static_cast<uint8_t>(actual.comparator.use));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.comparator.mode),
                          static_cast<uint8_t>(actual.comparator.mode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.comparator.polarity),
                          static_cast<uint8_t>(actual.comparator.polarity));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.comparator.latch),
                          static_cast<uint8_t>(actual.comparator.latch));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected.comparator.queue),
                          static_cast<uint8_t>(actual.comparator.queue));
  TEST_ASSERT_EQUAL_INT16(expected.comparator.lowThreshold,
                          actual.comparator.lowThreshold);
  TEST_ASSERT_EQUAL_INT16(expected.comparator.highThreshold,
                          actual.comparator.highThreshold);
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
  TEST_ASSERT_EQUAL_UINT8(18u, static_cast<uint8_t>(Err::CLOCK_STALLED));
}

void test_config_defaults() {
  Config cfg;
  TEST_ASSERT_NULL(cfg.i2cWrite);
  TEST_ASSERT_NULL(cfg.i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x48, cfg.i2cAddress);
  TEST_ASSERT_EQUAL_UINT16(50, cfg.i2cTimeoutMs);
  TEST_ASSERT_TRUE(cfg.strictInitVerify);
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

  const uint32_t writesBeforeSnapshot = bus.writeCalls;
  const uint32_t readsBeforeSnapshot = bus.readCalls;
  SettingsSnapshot snap;
  Status st = dev.getSettings(snap);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(writesBeforeSnapshot, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsBeforeSnapshot, bus.readCalls);
  TEST_ASSERT_TRUE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(snap.state));
  TEST_ASSERT_EQUAL_HEX8(0x4B, snap.i2cAddress);
  TEST_ASSERT_EQUAL_UINT32(10u, snap.i2cTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(3u, snap.offlineThreshold);
  TEST_ASSERT_TRUE(snap.strictInitVerify);
  TEST_ASSERT_TRUE(snap.hasNowMsHook);
  TEST_ASSERT_TRUE(snap.timebaseAvailable);
  TEST_ASSERT_TRUE(snap.hasGpioReadHook);
  TEST_ASSERT_TRUE(snap.hasCooperativeYieldHook);
  TEST_ASSERT_FALSE(snap.hardwareConfigDirty);
  TEST_ASSERT_TRUE(snap.hardwareConfigDirtyError.ok());
  TEST_ASSERT_EQUAL_HEX8(0x00, snap.hardwareConfigDirtyAddress);
  TEST_ASSERT_FALSE(snap.hardwareConfigUncertain);
  TEST_ASSERT_TRUE(snap.lastConfigApplyError.ok());
  TEST_ASSERT_FALSE(dev.isHardwareConfigDirty());
  TEST_ASSERT_FALSE(dev.isHardwareConfigUncertain());
  TEST_ASSERT_TRUE(dev.lastConfigApplyError().ok());
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

void test_begin_rejects_i2c_address_boundaries_without_bus_access() {
  const uint8_t badAddresses[] = {0x00, 0x47, 0x4C, 0x7F};
  for (uint8_t addr : badAddresses) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.i2cAddress = addr;

    Status st = dev.begin(cfg);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_FALSE(dev.isInitialized());
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
}

void test_begin_rejects_invalid_enum_values_without_bus_access() {
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.mux = static_cast<Mux>(8);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.gain = static_cast<Gain>(6);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.dataRate = static_cast<DataRate>(8);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.mode = static_cast<Mode>(2);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.compMode = static_cast<ComparatorMode>(2);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.compPolarity = static_cast<ComparatorPolarity>(2);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.compLatch = static_cast<ComparatorLatch>(2);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.compQueue = static_cast<ComparatorQueue>(4);
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
}

void test_begin_rejects_invalid_alert_ready_gpio_config_without_bus_access() {
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.alertRdyPin = -2;
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.alertRdyPin = 17;
    cfg.gpioRead = nullptr;
    Status st = dev.begin(cfg);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  }
}

void test_invalid_begin_preserves_existing_valid_binding() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config good = makeConfig(bus);
  good.i2cAddress = 0x4B;
  TEST_ASSERT_TRUE(dev.begin(good).ok());
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t successBefore = dev.totalSuccess();

  Config bad = makeConfig(bus);
  bad.i2cTimeoutMs = 0;
  Status st = dev.begin(bad);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_NOT_NULL(dev.getConfig().i2cWrite);
  TEST_ASSERT_NOT_NULL(dev.getConfig().i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x4B, dev.getConfig().i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(3u, dev.getConfig().offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(successBefore, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
  assertNoIoSince(bus, writesBefore, readsBefore);
}

void test_failed_begin_probe_keeps_valid_candidate_binding_for_retry() {
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
  TEST_ASSERT_NOT_NULL(dev.getConfig().i2cWrite);
  TEST_ASSERT_NOT_NULL(dev.getConfig().i2cWriteRead);
  TEST_ASSERT_EQUAL_HEX8(0x4B, dev.getConfig().i2cAddress);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::CONTINUOUS),
                          static_cast<uint8_t>(dev.getConfig().mode));
  TEST_ASSERT_EQUAL_UINT8(1u, dev.getConfig().offlineThreshold);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.bound);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNCONFIGURED),
                          static_cast<uint8_t>(snap.configurationState));
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(dev.state()),
                          static_cast<uint8_t>(dev.driverState()));
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

  const int32_t observed = static_cast<int32_t>(bus.reg[cmd::REG_CONFIG] ^ cmd::MASK_DR);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(observed, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_INT32(observed, dev.hardwareConfigDirtyError().detail);
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
  cfg.compThresholdLow = -1234;
  cfg.compThresholdHigh = 2345;
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "second begin write bus", -52);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-52, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cfg.compThresholdLow),
                           bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(cmd::HI_THRESH_DEFAULT, bus.reg[cmd::REG_HI_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(cmd::CONFIG_DEFAULT, bus.reg[cmd::REG_CONFIG]);
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -52);
}

void test_begin_partial_write_failure_preserves_dirty_address() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x4A;
  cfg.compThresholdLow = -1234;
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "second begin write bus", -152);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_HEX8(0x4A, dev.getConfig().i2cAddress);
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.hardwareConfigDirty);
  TEST_ASSERT_EQUAL_HEX8(0x4A, snap.hardwareConfigDirtyAddress);
  TEST_ASSERT_EQUAL_HEX8(0x4A, dev._hardwareConfigDirtyAddress);
}

void test_begin_failure_after_second_apply_write_preserves_dirty_diagnostic() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.compThresholdLow = -2222;
  cfg.compThresholdHigh = 2222;
  cfg.dataRate = DataRate::SPS_860;
  bus.failWriteOnCall = 3;
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "third begin write failure", -53);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-53, st.detail);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cfg.compThresholdLow),
                           bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cfg.compThresholdHigh),
                           bus.reg[cmd::REG_HI_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(cmd::CONFIG_DEFAULT, bus.reg[cmd::REG_CONFIG]);
  assertDirtyDiagnostic(dev, Err::I2C_ERROR, -53);
}

void test_begin_failure_on_third_apply_write_preserves_original_status_and_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.compThresholdLow = -3456;
  cfg.compThresholdHigh = 3456;
  cfg.dataRate = DataRate::SPS_860;
  bus.failWriteOnCall = 3;
  bus.failWriteStatus = Status::Error(Err::I2C_NACK_DATA, "config write nack", -58);

  Status st = dev.begin(cfg);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-58, st.detail);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cfg.compThresholdLow),
                           bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(cfg.compThresholdHigh),
                           bus.reg[cmd::REG_HI_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(cmd::CONFIG_DEFAULT, bus.reg[cmd::REG_CONFIG]);
  assertDirtyDiagnostic(dev, Err::I2C_NACK_DATA, -58);
}

void test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic() {
  const uint32_t failReadCalls[] = {2, 3, 4};
  for (uint32_t failCall : failReadCalls) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.strictInitVerify = true;
    bus.failReadOnCall = failCall;
    bus.failReadStatus = Status::Error(Err::I2C_TIMEOUT, "strict read timeout", -54);

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

void test_recover_success_clears_dirty_address() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.i2cAddress = 0x4B;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "recover high timeout", -153);

  Status st = dev.recover();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_HEX8(0x4B, snap.hardwareConfigDirtyAddress);

  resetIoCounters(bus);
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "unused", -1);
  st = dev.recover();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_EQUAL_HEX8(0x00, snap.hardwareConfigDirtyAddress);
  TEST_ASSERT_EQUAL_HEX8(0x00, dev._hardwareConfigDirtyAddress);
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

  const int32_t observed = static_cast<int32_t>(bus.reg[cmd::REG_CONFIG] ^ cmd::MASK_MODE);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(observed, st.detail);
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_INT32(observed, dev.hardwareConfigDirtyError().detail);
}

void test_recover_strict_low_threshold_mismatch_keeps_dirty_and_preserves_error() {
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
  bus.readXorMask[cmd::REG_LO_THRESH] = 0x0002;
  Status st = dev.recover();

  const int32_t observed = static_cast<int32_t>(bus.reg[cmd::REG_LO_THRESH] ^ 0x0002);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(observed, st.detail);
  assertDirtyDiagnostic(dev, Err::READBACK_MISMATCH, observed);
}

void test_recover_strict_high_threshold_mismatch_keeps_dirty_and_preserves_error() {
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
  bus.readXorMask[cmd::REG_HI_THRESH] = 0x0002;
  Status st = dev.recover();

  const int32_t observed = static_cast<int32_t>(bus.reg[cmd::REG_HI_THRESH] ^ 0x0002);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(observed, st.detail);
  assertDirtyDiagnostic(dev, Err::READBACK_MISMATCH, observed);
}

void test_recover_strict_readback_transport_failure_keeps_dirty_and_preserves_error() {
  const uint32_t failReadCalls[] = {2, 3, 4};
  for (uint32_t failCall : failReadCalls) {
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
    bus.failReadOnCall = failCall;
    bus.failReadStatus = Status::Error(Err::I2C_TIMEOUT, "strict read timeout", -90);
    Status st = dev.recover();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(-90, st.detail);
    assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -90);
  }
}

void test_probe_failure_does_not_update_health() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t beforeSuccess = dev.totalSuccess();
  const uint32_t beforeFailures = dev.totalFailures();
  const uint8_t beforeConsecutive = dev.consecutiveFailures();
  const DriverState beforeState = dev.state();
  const Status beforeLastError = dev.lastError();

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced probe timeout", -7);
  Status st = dev.probe();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(beforeSuccess, dev.totalSuccess());
  TEST_ASSERT_EQUAL_UINT32(beforeFailures, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(beforeConsecutive, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeState),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(beforeLastError.code),
                          static_cast<uint8_t>(dev.lastError().code));
  TEST_ASSERT_EQUAL_INT32(beforeLastError.detail, dev.lastError().detail);
}

void test_probe_before_begin_returns_not_initialized_without_bus_access() {
  ADS1115::ADS1115 dev;

  Status st = dev.probe();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
}

void test_probe_after_end_returns_not_initialized_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  dev.end();
  resetIoCounters(bus);

  Status st = dev.probe();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
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

void test_recover_probe_failure_updates_passive_health_diagnostics() {
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
  TEST_ASSERT_EQUAL_INT32(-9, dev.lastError().detail);
}

void test_recover_success_after_tracked_probe_failure_returns_ready() {
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

void test_tracked_failure_reaches_passive_offline_diagnostic_threshold() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -11);
  uint16_t value = 0;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT), static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_FALSE(dev.isOnline());
}

void test_passive_offline_diagnostic_does_not_gate_authorized_read() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::TIMEOUT, "forced timeout", -11);
  uint16_t value = 0;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(dev.readRegister16(cmd::REG_CONFIG, value).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  bus.readStatus = Status::Ok();
  const uint32_t readsBefore = bus.readCalls;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_GREATER_THAN_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
}

void test_pending_conversion_busy_precedes_passive_offline_diagnostic_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  dev._conversionStarted = true;
  forceOffline(dev);
  resetIoCounters(bus);

  Status st = dev.startConversion();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  st = dev.startConversion(Mux::AIN1_GND);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_passive_offline_diagnostic_allows_cached_readiness_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  dev._conversionReady = true;
  forceOffline(dev);
  resetIoCounters(bus);

  bool ready = true;
  Status st = dev.readConversionReady(ready);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(ready);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
}

void test_passive_offline_diagnostic_allows_due_conversion_service() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  dev._conversionStarted = true;
  dev._conversionReady = false;
  dev._conversionStartMs = bus.nowMs - dev.getConversionTimeMs();
  forceOffline(dev);
  resetIoCounters(bus);

  Status st = dev.service(bus.nowMs);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
}

void test_partial_recover_success_resets_offline_then_failure_degrades() {
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
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());

  bus.failWriteOnCall = 0;
  const uint32_t readsBefore = bus.readCalls;
  st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_GREATER_THAN_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
}

void test_tracked_read_preserves_distinct_i2c_errors_and_updates_health() {
  const Err errors[] = {
    Err::I2C_NACK_ADDR,
    Err::I2C_NACK_DATA,
    Err::I2C_TIMEOUT,
    Err::I2C_BUS,
    Err::I2C_ERROR
  };

  for (Err expected : errors) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.offlineThreshold = 10;
    TEST_ASSERT_TRUE(dev.begin(cfg).ok());
    resetIoCounters(bus);
    bus.readStatus = Status::Error(expected, "tracked read failure", -101);

    uint16_t value = 0;
    Status st = dev.readRegister16(cmd::REG_CONFIG, value);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(-101, st.detail);
    TEST_ASSERT_EQUAL_UINT32(1u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
    TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(dev.lastError().code));
    TEST_ASSERT_EQUAL_INT32(-101, dev.lastError().detail);
  }
}

void test_tracked_write_preserves_distinct_i2c_errors_and_updates_health() {
  const Err errors[] = {
    Err::I2C_NACK_ADDR,
    Err::I2C_NACK_DATA,
    Err::I2C_TIMEOUT,
    Err::I2C_BUS,
    Err::I2C_ERROR
  };

  for (Err expected : errors) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    Config cfg = makeConfig(bus);
    cfg.offlineThreshold = 10;
    TEST_ASSERT_TRUE(dev.begin(cfg).ok());
    const Gain oldGain = dev.getGain();
    resetIoCounters(bus);
    bus.writeStatus = Status::Error(expected, "tracked write failure", -102);

    Status st = dev.setGain(Gain::FSR_0_512V);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_INT32(-102, st.detail);
    TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
    TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldGain),
                            static_cast<uint8_t>(dev.getGain()));
    TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected),
                            static_cast<uint8_t>(dev.lastError().code));
    TEST_ASSERT_EQUAL_INT32(-102, dev.lastError().detail);
  }
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

void test_start_conversion_while_single_shot_pending_returns_busy_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  Status st = dev.startConversion();
  TEST_ASSERT_TRUE(st.inProgress());
  const uint32_t writesAfterStart = bus.writeCalls;
  const Mux cachedMux = dev.getMux();

  st = dev.startConversion();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesAfterStart, bus.writeCalls);

  st = dev.startConversion(Mux::AIN2_GND);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesAfterStart, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cachedMux),
                          static_cast<uint8_t>(dev.getMux()));
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
  bool ready = true;
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_FALSE(ready);

  // 13 ms elapsed across wrap (0xFFFFFFF8 -> 0x00000005), which is enough for 128 SPS.
  bus.nowMs = 5u;
  dev.tick(bus.nowMs);
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_TRUE(ready);

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
  bool ready = false;
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_TRUE(ready);

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
  TEST_ASSERT_EQUAL_INT32(-2, st.detail);
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

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
  TEST_ASSERT_FALSE(dev.conversionReady());
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
}

void test_conversion_ready_status_alias_preserves_transport_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_BUS, "forced bus failure", -64);
  bool ready = true;
  Status st = dev.conversionReady(ready);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-64, st.detail);
  TEST_ASSERT_FALSE(ready);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(dev.lastError().code));
}

void test_service_returns_ready_poll_failure_and_updates_health() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "service timeout", -65);
  Status st = dev.service(bus.nowMs);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-65, st.detail);
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(dev.lastError().code));
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev.lastErrorMs());
}

void test_tick_discards_status_but_updates_health_on_ready_poll_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "tick timeout", -66);
  dev.tick(bus.nowMs);

  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_INT32(-66, dev.lastError().detail);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::DEGRADED),
                          static_cast<uint8_t>(dev.state()));
}

void test_no_clock_direct_readiness_waits_for_service_timebase() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bool ready = true;
  Status st = dev.readConversionReady(ready);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(ready);

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.hasNowMsHook);
  TEST_ASSERT_FALSE(snap.timebaseAvailable);
  TEST_ASSERT_EQUAL_UINT32(0u, snap.conversionStartMs);

  st = dev.service(bus.nowMs);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_FALSE(ready);

  st = dev.service(bus.nowMs + dev.getConversionTimeMs());
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_TRUE(ready);
}

void test_no_clock_alert_ready_pin_uses_external_service_timebase() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.alertRdyPin = 17;
  cfg.gpioRead = fakeGpioRead;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.enableConversionReadyPin().ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());
  resetIoCounters(bus);

  bus.gpioLevel = false;  // Default comparator polarity is active-low.
  bus.nowMs += dev.getConversionTimeMs();
  bool ready = true;
  Status st = dev.readConversionReady(ready);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(ready);

  st = dev.service(bus.nowMs);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_FALSE(ready);

  st = dev.service(bus.nowMs + dev.getConversionTimeMs());
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(dev.readConversionReady(ready).ok());
  TEST_ASSERT_TRUE(ready);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_no_clock_health_timestamps_are_marked_unavailable() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.nowMs = nullptr;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  bus.readStatus = Status::Error(Err::I2C_TIMEOUT, "no-clock timeout", -67);
  uint16_t value = 0;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, dev.lastErrorMs());

  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_FALSE(snap.timebaseAvailable);
  TEST_ASSERT_FALSE(snap.hasNowMsHook);
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
  TEST_ASSERT_FALSE(ready);

  bus.nowMs += dev.getConversionTimeMs();
  st = dev.readConversionReady(ready);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_TRUE(ready);
}

void test_tick_marks_continuous_ready_without_config_poll_after_interval() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.dataRate = DataRate::SPS_128;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  bus.nowMs += 2U * dev.getConversionTimeMs();
  dev.tick(bus.nowMs);

  TEST_ASSERT_TRUE(dev._conversionReady);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
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

void test_read_blocking_continuous_is_rejected_without_bus_access() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x4A2B;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.dataRate = DataRate::SPS_8;
  cfg.cooperativeYield = fakeYieldAdvanceMs;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT16(0, raw);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.yieldCalls);
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
}

void test_continuous_blocking_rejection_precedes_diagnostic_transport_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.dataRate = DataRate::SPS_860;
  cfg.cooperativeYield = fakeYieldAdvanceMs;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.failReadOnCall = 1;
  bus.failReadStatus = Status::Error(Err::I2C_BUS, "continuous read bus", -96);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());

  st = dev.readLatestRaw(raw);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-96, st.detail);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(dev.lastError().code));
}

void test_single_shot_elapsed_os_busy_remains_not_ready() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());

  bus.nowMs += dev.getConversionTimeMs();
  bus.readXorMask[cmd::REG_CONFIG] = cmd::MASK_OS;
  bool ready = true;
  Status st = dev.readConversionReady(ready);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_FALSE(ready);
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
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

void test_poll_single_shot_max_one_wait_gate_and_raw_result() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0xFFFE;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  Status st = dev.startSingleShot(Mux::AIN2_GND);
  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  PollResult poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_WAIT_CONVERSION),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN0_GND),
                          static_cast<uint8_t>(dev.getMux()));

  poll = dev.pollSingleShot(bus.nowMs + dev.getConversionTimeMs() - 1u, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_WAIT_CONVERSION),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  bus.nowMs += dev.getConversionTimeMs();
  poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_READ_CONVERSION),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readCalls);

  poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_INT16(-2, dev.lastRawValue());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN2_GND),
                          static_cast<uint8_t>(dev.getMux()));
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(2u, bus.readCalls);
}

void test_poll_single_shot_budget_two_reads_ready_and_conversion() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x1234;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  PollResult poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);

  bus.nowMs += dev.getConversionTimeMs();
  poll = dev.pollSingleShot(bus.nowMs, 2);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(2u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_INT16(0x1234, dev.lastRawValue());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(2u, bus.readCalls);
}

void test_poll_single_shot_repeated_zero_budget_does_not_advance_or_touch_bus() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot(Mux::AIN1_GND).inProgress());
  for (uint8_t i = 0; i < 20; ++i) {
    PollResult poll = dev.pollSingleShot(bus.nowMs + i, 0);
    TEST_ASSERT_FALSE(poll.done);
    TEST_ASSERT_TRUE(poll.status.inProgress());
    TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_WRITE_CONFIG),
                            static_cast<uint8_t>(poll.state));
    TEST_ASSERT_TRUE(dev.jobActive());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN0_GND),
                            static_cast<uint8_t>(dev.getMux()));
  }
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  PollResult poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_WAIT_CONVERSION),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN0_GND),
                          static_cast<uint8_t>(dev.getMux()));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_poll_single_shot_large_budget_is_bounded_and_poll_after_complete_is_stable() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x8001;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  PollResult poll = dev.pollSingleShot(bus.nowMs, 255);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_WAIT_CONVERSION),
                          static_cast<uint8_t>(poll.state));

  bus.nowMs += dev.getConversionTimeMs();
  poll = dev.pollSingleShot(bus.nowMs, 255);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(2u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_INT16(static_cast<int16_t>(0x8001), dev.lastRawValue());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(2u, bus.readCalls);

  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;
  poll = dev.pollSingleShot(bus.nowMs, 255);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  assertNoIoSince(bus, writesBefore, readsBefore);
}

void test_poll_single_shot_ready_transport_failure_propagates() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  PollResult poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);

  bus.nowMs += dev.getConversionTimeMs();
  bus.failReadOnCall = 1;
  bus.failReadStatus = Status::Error(Err::I2C_BUS, "chunk ready bus error", -81);
  poll = dev.pollSingleShot(bus.nowMs, 1);

  // The readiness read failed while the conversion may still be running, so the
  // operation reconciles bus-silently before publishing the preserved error.
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-81, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::WAIT_IDLE_AFTER_ABANDON),
                          static_cast<uint8_t>(poll.state));

  bus.failReadOnCall = 0;
  const uint32_t readsBeforeQuietWait = bus.readCalls;
  poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  bus.nowMs += dev.getConversionTimeMs();
  poll = dev.pollSingleShot(bus.nowMs, 1);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(readsBeforeQuietWait, bus.readCalls);
  TEST_ASSERT_FALSE(dev._conversionStarted);

  // The driver is usable again: recovery is no longer blocked by a conversion
  // the driver could never observe completing.
  OperationResult terminal;
  TEST_ASSERT_TRUE(dev.takeResult(dev.activeOperationToken(), terminal).ok());
  TEST_ASSERT_TRUE(dev.recover().ok());
}

void test_start_apply_config_job_in_continuous_mode_is_supported() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev._conversionStarted);
  resetIoCounters(bus);

  Status st = dev.startApplyConfigJob();

  TEST_ASSERT_TRUE(st.inProgress());
  TEST_ASSERT_EQUAL_STRING("Operation started", st.msg);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::APPLY_PROFILE),
                          static_cast<uint8_t>(dev.operationKind()));
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_WRITE_LOW_THRESHOLD),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_start_apply_config_job_rejects_active_single_shot_conversion() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());
  resetIoCounters(bus);

  Status st = dev.startApplyConfigJob();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_STRING("Conversion may still be active", st.msg);
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_poll_apply_config_continuous_mode_finishes_with_continuous_timing_state() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  cfg.dataRate = DataRate::SPS_8;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  bus.nowMs += 77u;
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 3);

  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_TRUE(poll.status.inProgress());
  TEST_ASSERT_EQUAL_UINT8(3u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_VERIFY_LOW_THRESHOLD),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(3u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_EQUAL_UINT32(bus.nowMs, dev._conversionStartMs);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.readCalls);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(poll.token, result).ok());
  TEST_ASSERT_TRUE(result.status.ok());
}

void test_poll_apply_config_budget_and_strict_readback() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 2);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(2u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_WRITE_CONFIG),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(2u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(3u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_VERIFY_CONFIG),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(2u, bus.readCalls);

  poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_poll_apply_config_zero_budget_and_large_budget_clamp() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  for (uint8_t i = 0; i < 20; ++i) {
    PollResult poll = dev.pollApplyConfig(bus.nowMs + i, 0);
    TEST_ASSERT_FALSE(poll.done);
    TEST_ASSERT_TRUE(poll.status.inProgress());
    TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_WRITE_LOW_THRESHOLD),
                            static_cast<uint8_t>(poll.state));
    TEST_ASSERT_TRUE(dev.jobActive());
  }
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  PollResult poll = dev.pollApplyConfig(bus.nowMs, 255);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(3u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_VERIFY_LOW_THRESHOLD),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  poll = dev.pollApplyConfig(bus.nowMs, 255);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(3u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(3u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(3u, bus.readCalls);

  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;
  poll = dev.pollApplyConfig(bus.nowMs, 255);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::COMPLETE),
                          static_cast<uint8_t>(poll.state));
  assertNoIoSince(bus, writesBefore, readsBefore);
}

void test_wrong_job_poller_returns_busy_without_advancing_or_touching_bus() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_WRITE_CONFIG),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  const OperationToken readToken = dev.activeOperationToken();
  dev.cancelJob();
  OperationResult cancelledRead;
  TEST_ASSERT_TRUE(dev.takeResult(readToken, cancelledRead).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(cancelledRead.status.code));
  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  poll = dev.pollSingleShot(bus.nowMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_WRITE_LOW_THRESHOLD),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_cancel_job_publishes_terminal_result_before_restart() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot(Mux::AIN3_GND).inProgress());
  const OperationToken readToken = dev.activeOperationToken();
  dev.cancelJob();
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(dev.lastJobStatus().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::CANCELLED),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN0_GND),
                          static_cast<uint8_t>(dev.getMux()));

  OperationResult cancelledRead;
  TEST_ASSERT_TRUE(dev.takeResult(readToken, cancelledRead).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::CANCELLED),
                          static_cast<uint8_t>(cancelledRead.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(cancelledRead.status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::IDLE),
                          static_cast<uint8_t>(dev.jobState()));

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  const OperationToken applyToken = dev.activeOperationToken();
  dev.cancelJob();
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(dev.lastJobStatus().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::CANCELLED),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  OperationResult cancelledApply;
  TEST_ASSERT_TRUE(dev.takeResult(applyToken, cancelledApply).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::CANCELLED),
                          static_cast<uint8_t>(cancelledApply.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(cancelledApply.status.code));

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
}

void test_compatibility_staged_jobs_restart_after_terminal_result_ack() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  TEST_ASSERT_FALSE(dev.pollSingleShot(bus.nowMs, 1).done);
  bus.nowMs += dev.getConversionTimeMs();
  PollResult poll = dev.pollSingleShot(bus.nowMs, 2);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_TRUE(poll.token.valid());

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.startApplyConfigJob().code));
  OperationResult terminal;
  TEST_ASSERT_TRUE(dev.takeResult(poll.token, terminal).ok());
  TEST_ASSERT_TRUE(terminal.status.ok());

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_TRUE(poll.token.valid());

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.startSingleShot().code));
  TEST_ASSERT_TRUE(dev.takeResult(poll.token, terminal).ok());
  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  const OperationToken cancelToken = dev.activeOperationToken();
  dev.cancelJob();
  TEST_ASSERT_TRUE(dev.takeResult(cancelToken, terminal).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(terminal.status.code));
}

void test_active_job_service_advances_one_step_and_other_public_i2c_apis_stay_blocked() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  resetIoCounters(bus);

  bool ready = true;
  int16_t raw = 0;
  float volts = 0.0f;
  uint16_t reg = 0;
  int16_t low = 0;
  int16_t high = 0;

  Status serviceStatus = dev.service(bus.nowMs);
  TEST_ASSERT_TRUE(serviceStatus.inProgress());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_WRITE_HIGH_THRESHOLD),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  const uint32_t writesAfterService = bus.writeCalls;
  const uint32_t readsAfterService = bus.readCalls;

  assertBusyStatus(dev.probe());
  Status blocked = dev.recover();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(blocked.code));
  TEST_ASSERT_EQUAL_STRING("Operation already active", blocked.msg);
  blocked = dev.shutdown();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(blocked.code));
  TEST_ASSERT_EQUAL_STRING("Operation already active", blocked.msg);
  assertBusyStatus(dev.startConversion());
  assertBusyStatus(dev.startConversion(Mux::AIN2_GND));
  assertBusyStatus(dev.readConversionReady(ready));
  TEST_ASSERT_FALSE(ready);
  assertBusyStatus(dev.readRaw(raw));
  Status configUnknown = dev.readVoltage(volts);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CONFIG_UNKNOWN),
                          static_cast<uint8_t>(configUnknown.code));
  assertBusyStatus(dev.readLatestRaw(raw));
  assertBusyStatus(dev.readBlocking(raw, 200));
  configUnknown = dev.readBlockingVoltage(volts, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CONFIG_UNKNOWN),
                          static_cast<uint8_t>(configUnknown.code));
  assertBusyStatus(dev.setMux(Mux::AIN1_GND));
  assertBusyStatus(dev.setGain(Gain::FSR_0_512V));
  assertBusyStatus(dev.setDataRate(DataRate::SPS_860));
  assertBusyStatus(dev.setMode(Mode::CONTINUOUS));
  assertBusyStatus(dev.readConfig(reg));
  assertBusyStatus(dev.writeConfig(cmd::CONFIG_DEFAULT));
  assertBusyStatus(dev.setThresholds(-10, 10));
  assertBusyStatus(dev.getThresholds(low, high));
  assertBusyStatus(dev.setComparatorMode(ComparatorMode::WINDOW));
  assertBusyStatus(dev.setComparatorPolarity(ComparatorPolarity::ACTIVE_HIGH));
  assertBusyStatus(dev.setComparatorLatch(ComparatorLatch::LATCHING));
  assertBusyStatus(dev.setComparatorQueue(ComparatorQueue::ASSERT_2));
  assertBusyStatus(dev.enableConversionReadyPin());
  assertBusyStatus(dev.disableComparator());
  assertBusyStatus(dev.readRegister16(cmd::REG_CONFIG, reg));
  assertBusyStatus(dev.writeRegister16(cmd::REG_CONFIG, cmd::CONFIG_DEFAULT));

  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_WRITE_HIGH_THRESHOLD),
                          static_cast<uint8_t>(dev.jobState()));
  assertNoIoSince(bus, writesAfterService, readsAfterService);
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_active_job_preserves_invalid_param_precedence_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  resetIoCounters(bus);

  int16_t raw = 0;
  uint16_t reg = 0;

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.startConversion(static_cast<Mux>(8)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.readBlocking(raw, 0).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setMux(static_cast<Mux>(8)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setGain(static_cast<Gain>(6)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setDataRate(static_cast<DataRate>(8)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setMode(static_cast<Mode>(2)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setComparatorMode(static_cast<ComparatorMode>(2)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setComparatorPolarity(static_cast<ComparatorPolarity>(2)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setComparatorLatch(static_cast<ComparatorLatch>(2)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.setComparatorQueue(static_cast<ComparatorQueue>(4)).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.readRegister16(4, reg).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(dev.writeRegister16(cmd::REG_CONVERSION, 0).code));

  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_end_while_job_active_clears_job_and_later_poll_is_not_initialized() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  resetIoCounters(bus);

  dev.end();

  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_TRUE(dev.lastJobStatus().ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::IDLE),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  PollResult poll = dev.pollSingleShot(bus.nowMs, 3);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::IDLE),
                          static_cast<uint8_t>(poll.state));
}

void test_start_single_shot_rejects_invalid_mux_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  Status st = dev.startSingleShot(static_cast<Mux>(8));

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::IDLE),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_poll_single_shot_uncertain_write_failure_reconciles_bus_silently() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "single-shot write timeout", -91);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  const OperationToken token = dev.activeOperationToken();
  PollResult poll = dev.pollSingleShot(bus.nowMs, 3);

  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-91, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::WAIT_IDLE_AFTER_ABANDON),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(poll.operationState));
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(dev.lastError().code));
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -91);

  const uint32_t waitStartMs = bus.nowMs;
  poll = dev.pollSingleShot(waitStartMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  poll = dev.pollSingleShot(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128), 3);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_EQUAL_INT32(-91, result.status.detail);
  TEST_ASSERT_FALSE(result.sampleValid);
}

void test_poll_single_shot_first_write_address_nack_keeps_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_NACK_ADDR, "address absent", -94);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  PollResult poll = dev.pollSingleShot(bus.nowMs, 3);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-94, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_poll_single_shot_first_write_timeout_requires_idle_reconciliation() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "single-shot timeout", -95);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  const OperationToken token = dev.activeOperationToken();
  PollResult poll = dev.pollSingleShot(bus.nowMs, 3);

  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-95, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::WAIT_IDLE_AFTER_ABANDON),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(poll.operationState));
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -95);

  const uint32_t waitStartMs = bus.nowMs;
  poll = dev.pollSingleShot(waitStartMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  poll = dev.pollSingleShot(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128), 3);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_EQUAL_INT32(-95, result.status.detail);
}

void test_poll_single_shot_conversion_read_failure_updates_health_without_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);

  TEST_ASSERT_TRUE(dev.startSingleShot().inProgress());
  PollResult poll = dev.pollSingleShot(bus.nowMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);

  bus.nowMs += dev.getConversionTimeMs();
  bus.failReadOnCall = 2;
  bus.failReadStatus = Status::Error(Err::I2C_BUS, "conversion read bus", -92);
  poll = dev.pollSingleShot(bus.nowMs, 2);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-92, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(2u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_FALSE(dev.jobActive());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(1u, dev.consecutiveFailures());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(dev.lastError().code));
}

void test_poll_apply_config_first_write_address_nack_keeps_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_NACK_ADDR, "address absent", -93);

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 3);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-93, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(1u, dev.totalFailures());
}

void test_poll_apply_config_readback_mismatch_keeps_dirty_and_preserves_status() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.strictInitVerify = true;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.readXorMask[cmd::REG_LO_THRESH] = 0x0001;

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(3u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::APPLY_VERIFY_LOW_THRESHOLD),
                          static_cast<uint8_t>(poll.state));

  poll = dev.pollApplyConfig(bus.nowMs, 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(dev.hardwareConfigDirtyError().code));
  TEST_ASSERT_EQUAL_UINT32(0u, dev.totalFailures());
  TEST_ASSERT_EQUAL_UINT8(0u, dev.consecutiveFailures());
}

void test_poll_apply_config_partial_failure_marks_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "chunk high write timeout", -82);

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 3);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-82, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(2u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -82);
}

void test_poll_apply_config_first_write_failure_marks_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "chunk low write bus", -83);

  TEST_ASSERT_TRUE(dev.startApplyConfigJob().inProgress());
  PollResult poll = dev.pollApplyConfig(bus.nowMs, 1);

  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-83, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(1u, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::FAILED),
                          static_cast<uint8_t>(poll.state));
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -83);
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

void test_public_setters_reject_invalid_enum_values_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  const Config before = dev.getConfig();

  Status st = dev.setMux(static_cast<Mux>(8));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setGain(static_cast<Gain>(6));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setDataRate(static_cast<DataRate>(8));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setMode(static_cast<Mode>(2));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setComparatorMode(static_cast<ComparatorMode>(2));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setComparatorPolarity(static_cast<ComparatorPolarity>(2));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setComparatorLatch(static_cast<ComparatorLatch>(2));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.setComparatorQueue(static_cast<ComparatorQueue>(4));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  st = dev.startConversion(static_cast<Mux>(8));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));

  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.mux),
                          static_cast<uint8_t>(dev.getConfig().mux));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.gain),
                          static_cast<uint8_t>(dev.getConfig().gain));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.dataRate),
                          static_cast<uint8_t>(dev.getConfig().dataRate));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.mode),
                          static_cast<uint8_t>(dev.getConfig().mode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.compMode),
                          static_cast<uint8_t>(dev.getConfig().compMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.compPolarity),
                          static_cast<uint8_t>(dev.getConfig().compPolarity));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.compLatch),
                          static_cast<uint8_t>(dev.getConfig().compLatch));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(before.compQueue),
                          static_cast<uint8_t>(dev.getConfig().compQueue));
}

void test_write_config_normalizes_datasheet_pga_aliases_for_0_256v() {
  const uint16_t aliases[] = {cmd::PGA_0_256V_ALT1, cmd::PGA_0_256V_ALT2};
  // Both aliases are written as the canonical 101b encoding so the typed cache
  // matches hardware bit for bit and masked readbacks cannot report a mismatch
  // the driver produced itself.
  const uint16_t encodedValues[] = {5u, 5u};
  for (size_t i = 0; i < 2; ++i) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

    const uint16_t config = static_cast<uint16_t>(
        (cmd::CONFIG_DEFAULT & ~cmd::MASK_PGA) | aliases[i]);
    Status st = dev.writeConfig(config);

    TEST_ASSERT_TRUE(st.ok());
    TEST_ASSERT_EQUAL_UINT16(
        encodedValues[i], (bus.reg[cmd::REG_CONFIG] & cmd::MASK_PGA) >> cmd::BIT_PGA);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_256V),
                            static_cast<uint8_t>(dev.getGain()));
    TEST_ASSERT_TRUE(dev._conversionStarted);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNKNOWN),
                            static_cast<uint8_t>(dev.configurationState()));
  }
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

void test_thresholds_accept_and_reconstruct_int16_boundaries() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  Status st = dev.setThresholds(INT16_MIN, INT16_MAX);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(INT16_MIN), bus.reg[cmd::REG_LO_THRESH]);
  TEST_ASSERT_EQUAL_UINT16(static_cast<uint16_t>(INT16_MAX), bus.reg[cmd::REG_HI_THRESH]);

  int16_t low = 0;
  int16_t high = 0;
  st = dev.getThresholds(low, high);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(INT16_MIN, low);
  TEST_ASSERT_EQUAL_INT16(INT16_MAX, high);

  bus.reg[cmd::REG_LO_THRESH] = static_cast<uint16_t>(-1);
  bus.reg[cmd::REG_HI_THRESH] = static_cast<uint16_t>(1);
  st = dev.getThresholds(low, high);
  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(-1, low);
  TEST_ASSERT_EQUAL_INT16(1, high);
  TEST_ASSERT_EQUAL_INT16(-1, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(1, dev.getConfig().compThresholdHigh);
}

void test_lsb_voltage_for_all_gain_ranges() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Gain gains[] = {
    Gain::FSR_6_144V,
    Gain::FSR_4_096V,
    Gain::FSR_2_048V,
    Gain::FSR_1_024V,
    Gain::FSR_0_512V,
    Gain::FSR_0_256V
  };
  const float expected[] = {
    187.5e-6f,
    125.0e-6f,
    62.5e-6f,
    31.25e-6f,
    15.625e-6f,
    7.8125e-6f
  };

  for (size_t i = 0; i < sizeof(gains) / sizeof(gains[0]); ++i) {
    TEST_ASSERT_TRUE(dev.setGain(gains[i]).ok());
    TEST_ASSERT_FLOAT_WITHIN(0.0000001f, expected[i], dev.getLsbVoltage());
  }
}

void test_raw_to_voltage_for_all_gain_ranges_and_representative_codes() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const Gain gains[] = {
    Gain::FSR_6_144V,
    Gain::FSR_4_096V,
    Gain::FSR_2_048V,
    Gain::FSR_1_024V,
    Gain::FSR_0_512V,
    Gain::FSR_0_256V
  };
  const float lsb[] = {
    187.5e-6f,
    125.0e-6f,
    62.5e-6f,
    31.25e-6f,
    15.625e-6f,
    7.8125e-6f
  };
  const int16_t rawValues[] = {0, 1, -1, INT16_MAX, INT16_MIN};

  for (size_t gainIndex = 0; gainIndex < sizeof(gains) / sizeof(gains[0]); ++gainIndex) {
    TEST_ASSERT_TRUE(dev.setGain(gains[gainIndex]).ok());
    for (int16_t raw : rawValues) {
      const float expected = static_cast<float>(raw) * lsb[gainIndex];
      TEST_ASSERT_FLOAT_WITHIN(0.0002f, expected, dev.rawToVoltage(raw));
    }
  }
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

void test_config_only_setters_roll_back_each_cached_field_on_write_failure() {
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    const Mux oldValue = dev.getMux();
    bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -60);
    Status st = dev.setMux(Mux::AIN2_GND);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldValue),
                            static_cast<uint8_t>(dev.getMux()));
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    const DataRate oldValue = dev.getDataRate();
    bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -61);
    Status st = dev.setDataRate(DataRate::SPS_860);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldValue),
                            static_cast<uint8_t>(dev.getDataRate()));
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    const Mode oldMode = dev.getMode();
    const bool oldConversionStarted = dev._conversionStarted;
    bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -62);
    Status st = dev.setMode(Mode::CONTINUOUS);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldMode),
                            static_cast<uint8_t>(dev.getMode()));
    TEST_ASSERT_EQUAL(oldConversionStarted, dev._conversionStarted);
  }
}

void test_start_conversion_with_mux_rolls_back_cache_but_remains_active_when_ambiguous() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Mux oldMux = dev.getMux();
  resetIoCounters(bus);
  bus.writeStatus = Status::Error(Err::I2C_BUS, "start write failure", -63);

  Status st = dev.startConversion(Mux::AIN2_GND);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldMux),
                          static_cast<uint8_t>(dev.getMux()));
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNKNOWN),
                          static_cast<uint8_t>(dev.configurationState()));
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -63);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);

  bus.writeStatus = Status::Ok();
  st = dev.startConversion(Mux::AIN1_GND);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
}

void test_threshold_diagnostic_read_invalidates_full_profile_trust() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  AppliedProfileSnapshot before;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(before).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(before.state));

  int16_t low = 0;
  int16_t high = 0;
  TEST_ASSERT_TRUE(dev.getThresholds(low, high).ok());
  AppliedProfileSnapshot matching;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(matching).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(matching.state));
  TEST_ASSERT_EQUAL_UINT32(before.generation, matching.generation);

  bus.reg[cmd::REG_LO_THRESH] = static_cast<uint16_t>(-123);
  bus.reg[cmd::REG_HI_THRESH] = static_cast<uint16_t>(456);
  TEST_ASSERT_TRUE(dev.getThresholds(low, high).ok());
  TEST_ASSERT_EQUAL_INT16(-123, low);
  TEST_ASSERT_EQUAL_INT16(456, high);
  TEST_ASSERT_EQUAL_INT16(-123, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(456, dev.getConfig().compThresholdHigh);

  AppliedProfileSnapshot after;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(after).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNKNOWN),
                          static_cast<uint8_t>(after.state));
  TEST_ASSERT_EQUAL_UINT32(before.generation, after.generation);
  TEST_ASSERT_EQUAL_INT16(before.profile.comparator.lowThreshold,
                          after.profile.comparator.lowThreshold);
  TEST_ASSERT_EQUAL_INT16(before.profile.comparator.highThreshold,
                          after.profile.comparator.highThreshold);
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

void test_set_thresholds_second_write_failure_preserves_cache_and_dirty_reason() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const int16_t oldLow = dev.getConfig().compThresholdLow;
  const int16_t oldHigh = dev.getConfig().compThresholdHigh;
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "high threshold timeout", -64);

  Status st = dev.setThresholds(-321, 654);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-64, st.detail);
  TEST_ASSERT_EQUAL_INT16(oldLow, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(oldHigh, dev.getConfig().compThresholdHigh);
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -64);
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

void test_comparator_setters_roll_back_each_cached_field_on_write_failure() {
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    const ComparatorMode oldValue = dev.getComparatorMode();
    bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -65);
    Status st = dev.setComparatorMode(ComparatorMode::WINDOW);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldValue),
                            static_cast<uint8_t>(dev.getComparatorMode()));
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    const ComparatorPolarity oldValue = dev.getComparatorPolarity();
    bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -66);
    Status st = dev.setComparatorPolarity(ComparatorPolarity::ACTIVE_HIGH);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldValue),
                            static_cast<uint8_t>(dev.getComparatorPolarity()));
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    const ComparatorLatch oldValue = dev.getComparatorLatch();
    bus.writeStatus = Status::Error(Err::I2C_ERROR, "forced write failure", -67);
    Status st = dev.setComparatorLatch(ComparatorLatch::LATCHING);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                            static_cast<uint8_t>(st.code));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldValue),
                            static_cast<uint8_t>(dev.getComparatorLatch()));
  }
}

void test_disable_comparator_rolls_back_queue_on_write_failure() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  TEST_ASSERT_TRUE(dev.setComparatorQueue(ComparatorQueue::ASSERT_4).ok());
  const ComparatorQueue oldValue = dev.getComparatorQueue();
  resetIoCounters(bus);
  bus.writeStatus = Status::Error(Err::I2C_ERROR, "disable comparator write failure", -70);

  Status st = dev.disableComparator();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldValue),
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

void test_enable_conversion_ready_pin_partial_failure_rolls_back_cache_and_dirty_reason() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Config oldConfig = dev.getConfig();
  resetIoCounters(bus);
  bus.failWriteOnCall = 2;
  bus.failWriteStatus = Status::Error(Err::I2C_NACK_DATA, "ready pin partial nack", -68);

  Status st = dev.enableConversionReadyPin();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT16(oldConfig.compThresholdLow, dev.getConfig().compThresholdLow);
  TEST_ASSERT_EQUAL_INT16(oldConfig.compThresholdHigh, dev.getConfig().compThresholdHigh);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldConfig.compQueue),
                          static_cast<uint8_t>(dev.getConfig().compQueue));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldConfig.compMode),
                          static_cast<uint8_t>(dev.getConfig().compMode));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldConfig.compLatch),
                          static_cast<uint8_t>(dev.getConfig().compLatch));
  assertDirtyDiagnostic(dev, Err::I2C_NACK_DATA, -68);
}

void test_apply_config_failure_on_first_write_marks_hardware_dirty() {
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
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -41);
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

void test_set_thresholds_first_write_failure_marks_hardware_dirty() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "low threshold bus", -46);

  Status st = dev.setThresholds(-123, 456);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -46);
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

void test_failed_config_only_setter_preserves_prior_dirty_reason() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  resetIoCounters(bus);
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, 0x1234).ok());
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);

  resetIoCounters(bus);
  bus.writeStatus = Status::Error(Err::I2C_BUS, "config write bus", -69);
  Status st = dev.setDataRate(DataRate::SPS_860);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
}

void test_failed_config_only_setter_marks_dirty_when_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Mux oldMux = dev.getMux();
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "config write timeout", -76);

  Status st = dev.setMux(Mux::AIN2_GND);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldMux),
                          static_cast<uint8_t>(dev.getMux()));
  assertDirtyDiagnostic(dev, Err::I2C_TIMEOUT, -76);
}

void test_failed_config_only_setter_address_nack_keeps_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Mux oldMux = dev.getMux();
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_NACK_ADDR, "address nack", -80);

  Status st = dev.setMux(Mux::AIN2_GND);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_ADDR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldMux),
                          static_cast<uint8_t>(dev.getMux()));
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
}

void test_failed_write_config_marks_dirty_when_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Config cached = dev.getConfig();
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_BUS, "writeConfig bus", -77);
  const uint16_t config = static_cast<uint16_t>(cmd::CONFIG_DEFAULT ^ cmd::MASK_DR);

  Status st = dev.writeConfig(config);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(cached.dataRate),
                          static_cast<uint8_t>(dev.getDataRate()));
  assertDirtyDiagnostic(dev, Err::I2C_BUS, -77);
}

void test_failed_shutdown_marks_dirty_when_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_ERROR, "shutdown write failed", -78);

  Status st = dev.shutdown();

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_ERROR),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::CONTINUOUS),
                          static_cast<uint8_t>(dev.getMode()));
  assertDirtyDiagnostic(dev, Err::I2C_ERROR, -78);
}

void test_failed_start_conversion_mux_override_marks_dirty_when_clean() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
  const Mux oldMux = dev.getMux();
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_NACK_DATA, "start mux write nack", -79);

  Status st = dev.startConversion(Mux::AIN1_GND);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_NACK_DATA),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(oldMux),
                          static_cast<uint8_t>(dev.getMux()));
  assertDirtyDiagnostic(dev, Err::I2C_NACK_DATA, -79);
}

void test_failed_config_and_comparator_setters_preserve_prior_dirty_reason() {
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, 0x1234).ok());
    resetIoCounters(bus);
    bus.writeStatus = Status::Error(Err::I2C_BUS, "mux write bus", -71);

    Status st = dev.setMux(Mux::AIN2_GND);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, 0x1234).ok());
    resetIoCounters(bus);
    bus.writeStatus = Status::Error(Err::I2C_BUS, "gain write bus", -72);

    Status st = dev.setGain(Gain::FSR_0_512V);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, 0x1234).ok());
    resetIoCounters(bus);
    bus.writeStatus = Status::Error(Err::I2C_BUS, "mode write bus", -73);

    Status st = dev.setMode(Mode::CONTINUOUS);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, 0x1234).ok());
    resetIoCounters(bus);
    bus.writeStatus = Status::Error(Err::I2C_BUS, "comparator write bus", -74);

    Status st = dev.setComparatorMode(ComparatorMode::WINDOW);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  }
  {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());
    TEST_ASSERT_TRUE(dev.setComparatorQueue(ComparatorQueue::ASSERT_2).ok());
    TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG, 0x1234).ok());
    resetIoCounters(bus);
    bus.writeStatus = Status::Error(Err::I2C_BUS, "disable comparator bus", -75);

    Status st = dev.disableComparator();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                            static_cast<uint8_t>(st.code));
    assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
  }
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

// Option A contract: raw register writes are diagnostic access. They update
// hardware but leave typed cache unchanged and mark cache/hardware state dirty.
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

void test_passive_offline_diagnostic_allows_authorized_raw_write() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  forceOffline(dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  resetIoCounters(bus);
  const uint32_t writesBefore = bus.writeCalls;
  const uint16_t rawConfig = static_cast<uint16_t>(cmd::CONFIG_DEFAULT ^ cmd::MASK_DR);
  Status st = dev.writeRegister16(cmd::REG_CONFIG, rawConfig);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(writesBefore + 1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT16(rawConfig, bus.reg[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  assertDirtyDiagnostic(dev, Err::HARDWARE_CONFIG_DIRTY, cmd::REG_CONFIG);
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

void test_read_blocking_stalled_clock_enters_bus_silent_reconciliation() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CLOCK_STALLED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(ADS1115::ADS1115::kMaxSameTickPolls, st.detail);
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(dev.operationState()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::WAIT_IDLE_AFTER_ABANDON),
                          static_cast<uint8_t>(dev.jobState()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNKNOWN),
                          static_cast<uint8_t>(dev.configurationState()));

  const OperationToken token = dev.activeOperationToken();
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t waitStartMs = bus.nowMs;
  PollResult poll = dev.poll(waitStartMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128), 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  assertNoIoSince(bus, writesBefore, readsBefore);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CLOCK_STALLED),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_FALSE(result.sampleValid);
}

void test_read_blocking_rejects_invalid_timeout_without_starting_conversion() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 0);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);

  st = dev.readBlocking(raw, UINT32_MAX);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_FALSE(dev._conversionStarted);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
}

void test_read_blocking_success_uses_cooperative_yield_cadence() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x2222;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.cooperativeYield = fakeYieldAdvanceMs;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(0x2222, raw);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(2u, bus.readCalls);
  TEST_ASSERT_TRUE(bus.yieldCalls >= dev.getConversionTimeMs());
}

void test_read_blocking_tolerates_repeated_same_tick_before_clock_advances() {
  FakeBus bus;
  bus.reg[cmd::REG_CONVERSION] = 0x3333;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.cooperativeYield = fakeYieldAdvanceEveryOtherCall;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_INT16(0x3333, raw);
  TEST_ASSERT_TRUE(bus.yieldCalls > dev.getConversionTimeMs());
}

void test_read_blocking_rejects_joining_existing_direct_conversion_without_bus_access() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.cooperativeYield = fakeYield;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());
  bus.nowMs += dev.getConversionTimeMs();
  resetIoCounters(bus);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(0u, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT32(0u, bus.writeCalls);
  TEST_ASSERT_TRUE(dev._conversionStarted);
}

void test_read_blocking_times_out_with_advancing_clock_while_os_busy() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.cooperativeYield = fakeYieldAdvanceMs;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.readXorMask[cmd::REG_CONFIG] = cmd::MASK_OS;

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 12);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_TRUE(dev._conversionStarted);
  TEST_ASSERT_FALSE(dev._conversionReady);
  TEST_ASSERT_TRUE(dev.jobActive());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(dev.operationState()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNKNOWN),
                          static_cast<uint8_t>(dev.configurationState()));
  TEST_ASSERT_TRUE(bus.readCalls <= 2u);

  const OperationToken token = dev.activeOperationToken();
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;
  const uint32_t waitStartMs = bus.nowMs;
  PollResult poll = dev.poll(waitStartMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128), 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0u, poll.instructionsUsed);
  assertNoIoSince(bus, writesBefore, readsBefore);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(result.status.code));
}

void test_read_blocking_propagates_ready_poll_transport_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.mode = Mode::SINGLE_SHOT;
  cfg.cooperativeYield = fakeYieldAdvanceMs;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  bus.failReadOnCall = 1;
  bus.failReadStatus = Status::Error(Err::I2C_BUS, "blocking poll failed", -68);

  int16_t raw = 0;
  Status st = dev.readBlocking(raw, 200);

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_BUS),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_INT32(-68, st.detail);
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

void test_bus_silent_end_then_register_access_does_not_touch_bus() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.begin(makeConfig(bus)).ok());

  const uint32_t writesAfterBegin = bus.writeCalls;
  const uint32_t readsAfterBegin = bus.readCalls;

  dev.end();
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);

  uint16_t value = 0;
  Status st = dev.readRegister16(cmd::REG_CONFIG, value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(readsAfterBegin, bus.readCalls);

  st = dev.writeRegister16(cmd::REG_CONFIG, cmd::CONFIG_DEFAULT);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(st.code));
  TEST_ASSERT_EQUAL_UINT32(writesAfterBegin, bus.writeCalls);
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

void test_shutdown_proceeds_when_passive_health_is_offline() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  Config cfg = makeConfig(bus);
  cfg.offlineThreshold = 1;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  forceOffline(dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  resetIoCounters(bus);

  Status st = dev.shutdown();

  TEST_ASSERT_TRUE(st.ok());
  TEST_ASSERT_EQUAL_UINT32(1u, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(1u, bus.readCalls);
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SINGLE_SHOT),
                          static_cast<uint8_t>(dev.getMode()));
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
  forceOffline(dev);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));

  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;
  dev.end();
  TEST_ASSERT_EQUAL_UINT32(writesBefore, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(readsBefore, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::UNINIT),
                          static_cast<uint8_t>(dev.state()));
}

void test_scaled_reads_report_lifecycle_and_continuous_mode_preconditions() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  float volts = 0.0f;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(dev.readVoltage(volts).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::NOT_INITIALIZED),
      static_cast<uint8_t>(dev.readBlockingVoltage(volts, 200).code));

  Config cfg = makeConfig(bus);
  cfg.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(dev.begin(cfg).ok());
  resetIoCounters(bus);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION),
      static_cast<uint8_t>(dev.readVoltage(volts).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::UNSUPPORTED_OPERATION),
      static_cast<uint8_t>(dev.readBlockingVoltage(volts, 200).code));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);
}

void test_owner_safe_pure_helpers_cover_boundaries_and_exact_units() {
  const DataRate rates[] = {
      DataRate::SPS_8, DataRate::SPS_16, DataRate::SPS_32, DataRate::SPS_64,
      DataRate::SPS_128, DataRate::SPS_250, DataRate::SPS_475, DataRate::SPS_860};
  const uint16_t expectedSps[] = {8, 16, 32, 64, 128, 250, 475, 860};
  const uint32_t expectedWorstCaseUs[] = {
      139889, 70445, 35723, 18362, 9681, 5445, 3340, 2292};
  for (size_t i = 0; i < sizeof(rates) / sizeof(rates[0]); ++i) {
    TEST_ASSERT_EQUAL_UINT16(expectedSps[i], dataRateSps(rates[i]));
    TEST_ASSERT_EQUAL_UINT32(expectedWorstCaseUs[i],
                             worstCaseConversionTimeUs(rates[i]));
  }
  TEST_ASSERT_EQUAL_UINT16(0U, dataRateSps(static_cast<DataRate>(8)));
  TEST_ASSERT_EQUAL_UINT32(0U, worstCaseConversionTimeUs(static_cast<DataRate>(8)));

  const Gain gains[] = {
      Gain::FSR_6_144V, Gain::FSR_4_096V, Gain::FSR_2_048V,
      Gain::FSR_1_024V, Gain::FSR_0_512V, Gain::FSR_0_256V};
  const int32_t fullScaleUv[] = {
      6144000, 4096000, 2048000, 1024000, 512000, 256000};
  const int32_t oneCodeUv[] = {188, 125, 63, 31, 16, 8};
  const int32_t positiveLimitUv[] = {
      6143813, 4095875, 2047938, 1023969, 511984, 255992};
  for (size_t i = 0; i < sizeof(gains) / sizeof(gains[0]); ++i) {
    TEST_ASSERT_EQUAL_INT32(fullScaleUv[i], gainFullScaleMicrovolts(gains[i]));
    int32_t out = 123;
    TEST_ASSERT_TRUE(rawToMicrovolts(0, gains[i], out).ok());
    TEST_ASSERT_EQUAL_INT32(0, out);
    TEST_ASSERT_TRUE(rawToMicrovolts(1, gains[i], out).ok());
    TEST_ASSERT_EQUAL_INT32(oneCodeUv[i], out);
    TEST_ASSERT_TRUE(rawToMicrovolts(-1, gains[i], out).ok());
    TEST_ASSERT_EQUAL_INT32(-oneCodeUv[i], out);
    TEST_ASSERT_TRUE(rawToMicrovolts(INT16_MAX, gains[i], out).ok());
    TEST_ASSERT_EQUAL_INT32(positiveLimitUv[i], out);
    TEST_ASSERT_TRUE(rawToMicrovolts(INT16_MIN, gains[i], out).ok());
    TEST_ASSERT_EQUAL_INT32(-fullScaleUv[i], out);
  }
  int32_t invalidOut = 99;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::INVALID_PARAM),
      static_cast<uint8_t>(rawToMicrovolts(1, static_cast<Gain>(6), invalidOut).code));
  TEST_ASSERT_EQUAL_INT32(0, invalidOut);
  TEST_ASSERT_EQUAL_INT32(0, gainFullScaleMicrovolts(static_cast<Gain>(6)));

  TEST_ASSERT_FALSE(isSingleEnded(Mux::AIN0_AIN1));
  TEST_ASSERT_TRUE(isSingleEnded(Mux::AIN0_GND));
  TEST_ASSERT_EQUAL_INT8(0, positiveInput(Mux::AIN0_AIN3));
  TEST_ASSERT_EQUAL_INT8(3, negativeInput(Mux::AIN0_AIN3));
  TEST_ASSERT_EQUAL_INT8(3, positiveInput(Mux::AIN3_GND));
  TEST_ASSERT_EQUAL_INT8(-1, negativeInput(Mux::AIN3_GND));
  TEST_ASSERT_EQUAL_INT8(-1, positiveInput(static_cast<Mux>(8)));
  TEST_ASSERT_EQUAL_INT8(-2, negativeInput(static_cast<Mux>(8)));

  TEST_ASSERT_EQUAL_UINT32(145U, operationDeadlineMs(1, DataRate::SPS_8, 5));
  TEST_ASSERT_EQUAL_UINT32(570U, operationDeadlineMs(4, DataRate::SPS_8, 10));
  TEST_ASSERT_EQUAL_UINT32(0U, operationDeadlineMs(0, DataRate::SPS_128, 10));
  TEST_ASSERT_EQUAL_UINT32(
      0U, operationDeadlineMs(1, static_cast<DataRate>(8), 10));
  TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(INT32_MAX),
                           operationDeadlineMs(255, DataRate::SPS_8, UINT32_MAX));
}

void test_owner_safe_profile_validation_boundaries() {
  DeviceProfile profile = makeDeviceProfile();
  TEST_ASSERT_TRUE(validateDeviceProfile(profile).ok());

  profile.i2cAddress = 0x47;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(validateDeviceProfile(profile).code));
  profile = makeDeviceProfile();
  profile.mode = Mode::CONTINUOUS;
  TEST_ASSERT_TRUE(validateDeviceProfile(profile).ok());
  profile.mode = static_cast<Mode>(2);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(validateDeviceProfile(profile).code));

  ComparatorProfile comparator;
  comparator.use = ComparatorUse::THRESHOLD;
  comparator.queue = ComparatorQueue::ASSERT_1;
  comparator.lowThreshold = 10;
  comparator.highThreshold = 10;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(validateComparatorProfile(comparator).code));
  comparator.highThreshold = 11;
  TEST_ASSERT_TRUE(validateComparatorProfile(comparator).ok());
  comparator.use = ComparatorUse::CONVERSION_READY;
  comparator.mode = ComparatorMode::TRADITIONAL;
  comparator.latch = ComparatorLatch::NON_LATCHING;
  comparator.queue = ComparatorQueue::ASSERT_1;
  comparator.lowThreshold = 0;
  comparator.highThreshold = INT16_MIN;
  TEST_ASSERT_TRUE(validateComparatorProfile(comparator).ok());
  comparator.highThreshold = INT16_MAX;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_CONFIG),
                          static_cast<uint8_t>(validateComparatorProfile(comparator).code));

  ChannelRequest request;
  TEST_ASSERT_TRUE(validateChannelRequest(request).ok());
  request.mux = static_cast<Mux>(8);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::INVALID_PARAM),
                          static_cast<uint8_t>(validateChannelRequest(request).code));
}

void test_owner_safe_bind_and_unbind_are_zero_i2c() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;

  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).ok());
  assertNoIoSince(bus, writesBefore, readsBefore);
  SettingsSnapshot snap;
  TEST_ASSERT_TRUE(dev.getSettings(snap).ok());
  TEST_ASSERT_TRUE(snap.bound);
  TEST_ASSERT_FALSE(snap.initialized);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNCONFIGURED),
                          static_cast<uint8_t>(snap.configurationState));

  OperationToken token;
  TEST_ASSERT_TRUE(dev.startInitialize(100, 1000, token).inProgress());
  assertNoIoSince(bus, writesBefore, readsBefore);
  dev.unbind();
  assertNoIoSince(bus, writesBefore, readsBefore);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::NOT_INITIALIZED),
                          static_cast<uint8_t>(dev.poll(100, 1).status.code));
}

void test_owner_safe_initialize_uses_one_transfer_polls_and_commits_generation_after_readback() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).ok());
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startInitialize(100, 1000, token).inProgress());

  for (uint8_t step = 0; step < 6U; ++step) {
    const uint32_t writesBefore = bus.writeCalls;
    const uint32_t readsBefore = bus.readCalls;
    PollResult poll = dev.poll(100, 1);
    TEST_ASSERT_FALSE(poll.done);
    TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
    TEST_ASSERT_EQUAL_UINT32(
        1U, (bus.writeCalls - writesBefore) + (bus.readCalls - readsBefore));
    AppliedProfileSnapshot applied;
    TEST_ASSERT_TRUE(dev.getAppliedProfile(applied).ok());
    TEST_ASSERT_EQUAL_UINT32(0U, applied.generation);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::APPLYING),
                            static_cast<uint8_t>(applied.state));
  }

  const uint32_t writesBefore = bus.writeCalls;
  const uint32_t readsBefore = bus.readCalls;
  PollResult poll = dev.poll(100, 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(
      1U, (bus.writeCalls - writesBefore) + (bus.readCalls - readsBefore));
  TEST_ASSERT_EQUAL_UINT32(3U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(4U, bus.readCalls);

  AppliedProfileSnapshot applied;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(applied).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(applied.state));
  TEST_ASSERT_EQUAL_UINT32(1U, applied.generation);
  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::SUCCEEDED),
                          static_cast<uint8_t>(result.state));
}

void test_owner_safe_initialize_readback_failure_keeps_generation_zero_and_unknown() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).ok());
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startInitialize(100, 1000, token).inProgress());
  bus.readXorMask[cmd::REG_CONFIG] = cmd::MASK_DR;

  PollResult poll;
  for (uint8_t step = 0; step < 7U; ++step) {
    poll = dev.poll(100, 1);
  }
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::READBACK_MISMATCH),
                          static_cast<uint8_t>(poll.status.code));
  AppliedProfileSnapshot applied;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(applied).ok());
  TEST_ASSERT_EQUAL_UINT32(0U, applied.generation);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNKNOWN),
                          static_cast<uint8_t>(applied.state));
  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_TRUE(result.hardwareStateUncertain);
  TEST_ASSERT_FALSE(result.sampleValid);
}

void test_owner_safe_poll_clamps_callback_timeout_to_deadline_remaining() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).ok());

  OperationToken token;
  TEST_ASSERT_TRUE(dev.startInitialize(100, 103, token).inProgress());
  PollResult poll = dev.poll(101, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(2U, bus.lastReadTimeoutMs);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CancelDisposition::CANCELLED_BEFORE_EFFECT),
      static_cast<uint8_t>(dev.cancelActiveOperation()));
  OperationResult cancelled;
  TEST_ASSERT_TRUE(dev.takeResult(token, cancelled).ok());

  resetIoCounters(bus);
  TEST_ASSERT_TRUE(dev.startInitialize(200, 300, token).inProgress());
  poll = dev.poll(200, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT32(10U, bus.lastReadTimeoutMs);
  (void)dev.cancelActiveOperation();
  TEST_ASSERT_TRUE(dev.takeResult(token, cancelled).ok());
}

void test_owner_safe_multi_callback_poll_partitions_remaining_deadline_budget() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).ok());

  OperationToken token;
  TEST_ASSERT_TRUE(dev.startInitialize(100U, 109U, token).inProgress());
  PollResult poll = dev.poll(100U, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(3U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(3U, bus.lastReadTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(3U, bus.lastWriteTimeoutMs);
  (void)dev.cancelActiveOperation();
  OperationResult cancelled;
  TEST_ASSERT_TRUE(dev.takeResult(token, cancelled).ok());

  resetIoCounters(bus);
  TEST_ASSERT_TRUE(dev.startInitialize(200U, 202U, token).inProgress());
  poll = dev.poll(200U, 3);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(2U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.lastReadTimeoutMs);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.lastWriteTimeoutMs);
  (void)dev.cancelActiveOperation();
  TEST_ASSERT_TRUE(dev.takeResult(token, cancelled).ok());
}

void test_owner_safe_reinitialize_and_rebind_reject_active_direct_conversion() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  TEST_ASSERT_TRUE(dev.startConversion().inProgress());
  const uint32_t writesAfterStart = bus.writeCalls;
  const uint32_t readsAfterStart = bus.readCalls;

  OperationToken token;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.startInitialize(200, 1000, token).code));
  TEST_ASSERT_FALSE(token.valid());
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).code));
  TEST_ASSERT_TRUE(dev.isBound());
  TEST_ASSERT_TRUE(dev.isInitialized());
  assertNoIoSince(bus, writesAfterStart, readsAfterStart);
}

void test_owner_safe_initialize_cancel_after_each_pending_stage_is_bus_silent() {
  for (uint8_t completedTransfers = 0; completedTransfers < 7U;
       ++completedTransfers) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), makeDeviceProfile()).ok());
    OperationToken token;
    TEST_ASSERT_TRUE(dev.startInitialize(100, 1000, token).inProgress());

    for (uint8_t step = 0; step < completedTransfers; ++step) {
      const PollResult poll = dev.poll(100, 1);
      TEST_ASSERT_FALSE(poll.done);
      TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
    }
    const uint32_t writesBeforeCancel = bus.writeCalls;
    const uint32_t readsBeforeCancel = bus.readCalls;
    const bool writeEffectPossible = completedTransfers >= 2U;
    const CancelDisposition disposition = dev.cancelActiveOperation();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(writeEffectPossible
                                 ? CancelDisposition::CANCELLED_AFTER_EFFECT
                                 : CancelDisposition::CANCELLED_BEFORE_EFFECT),
        static_cast<uint8_t>(disposition));
    assertNoIoSince(bus, writesBeforeCancel, readsBeforeCancel);

    OperationResult result;
    TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::CANCELLED),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                            static_cast<uint8_t>(result.status.code));
    TEST_ASSERT_EQUAL(writeEffectPossible, result.hardwareStateUncertain);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(writeEffectPossible
                                 ? ConfigurationState::UNKNOWN
                                 : ConfigurationState::UNCONFIGURED),
        static_cast<uint8_t>(dev.configurationState()));
    TEST_ASSERT_EQUAL(writeEffectPossible, dev.hardwareConfigDirty());
  }
}

void test_owner_safe_tokened_read_publishes_atomic_sample_exactly_once() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  bus.reg[cmd::REG_CONVERSION] = static_cast<uint16_t>(-2);

  ChannelRequest request;
  request.channelId = 42;
  request.mux = Mux::AIN2_GND;
  request.gain = Gain::FSR_0_512V;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200, 400, token).inProgress());
  TEST_ASSERT_TRUE(token.valid());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);

  PollResult poll = dev.poll(200, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.writeCalls);
  const uint32_t readyMs = 200U + ownerConversionTimeMs(DataRate::SPS_128);
  poll = dev.poll(readyMs - 1U, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);

  poll = dev.poll(readyMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
  poll = dev.poll(readyMs, 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT32(2U, bus.readCalls);

  OperationResult result;
  OperationToken wrongToken{token.value + 1U};
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TOKEN_MISMATCH),
                          static_cast<uint8_t>(dev.takeResult(wrongToken, result).code));
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::READ_SINGLE_SHOT),
                          static_cast<uint8_t>(result.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::SUCCEEDED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_TRUE(result.sampleValid);
  TEST_ASSERT_EQUAL_INT16(-2, result.sample.rawCode);
  TEST_ASSERT_EQUAL_INT32(-31, result.sample.microvolts);
  TEST_ASSERT_EQUAL_UINT16(42U, result.sample.channelId);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mux::AIN2_GND),
                          static_cast<uint8_t>(result.sample.mux));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gain::FSR_0_512V),
                          static_cast<uint8_t>(result.sample.gain));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DataRate::SPS_128),
                          static_cast<uint8_t>(result.sample.dataRate));
  TEST_ASSERT_TRUE((result.sample.flags &
                    static_cast<uint16_t>(SampleFlag::CONFIG_VERIFIED)) != 0U);
  TEST_ASSERT_EQUAL_UINT32(2U, result.sample.configGeneration);
  TEST_ASSERT_EQUAL_UINT32(1U, result.sample.sequence);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::RESULT_NOT_AVAILABLE),
                          static_cast<uint8_t>(dev.takeResult(token, result).code));
}

void test_owner_safe_deadline_is_wrap_safe_and_reconciles_without_i2c() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  const uint32_t startMs = 0xFFFFFFF0U;
  const uint32_t deadlineMs = 0x00000020U;
  ChannelRequest request;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, startMs, deadlineMs, token).inProgress());
  TEST_ASSERT_FALSE(dev.poll(startMs, 1).done);
  bus.readXorMask[cmd::REG_CONFIG] = cmd::MASK_OS;

  const uint32_t firstReadyMs = startMs + ownerConversionTimeMs(DataRate::SPS_128);
  PollResult poll = dev.poll(firstReadyMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  const uint32_t writesBeforeTimeout = bus.writeCalls;
  const uint32_t readsBeforeTimeout = bus.readCalls;
  poll = dev.poll(deadlineMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(poll.operationState));
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeTimeout, readsBeforeTimeout);

  poll = dev.poll(deadlineMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeTimeout, readsBeforeTimeout);

  poll = dev.poll(deadlineMs + ownerConversionTimeMs(DataRate::SPS_128), 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::TIMED_OUT),
                          static_cast<uint8_t>(poll.operationState));
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeTimeout, readsBeforeTimeout);
  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::TIMEOUT),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_FALSE(result.sampleValid);
}

void test_owner_safe_cancel_before_io_is_terminal_and_bus_silent() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  ChannelRequest request;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200, 400, token).inProgress());

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CancelDisposition::CANCELLED_BEFORE_EFFECT),
                          static_cast<uint8_t>(dev.cancelActiveOperation()));
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);
  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::CANCELLED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_FALSE(result.sampleValid);

  OperationToken restartToken;
  TEST_ASSERT_TRUE(dev.startRead(request, 201, 401, restartToken).inProgress());
  TEST_ASSERT_TRUE(restartToken.valid());
}

void test_owner_safe_cancel_after_confirmed_start_waits_bus_silently_and_blocks_restart() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  ChannelRequest request;
  request.mux = Mux::AIN1_GND;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200, 400, token).inProgress());
  TEST_ASSERT_FALSE(dev.poll(200, 1).done);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CancelDisposition::RECONCILIATION_REQUIRED),
      static_cast<uint8_t>(dev.cancelActiveOperation()));
  const uint32_t writesAfterCancel = bus.writeCalls;
  const uint32_t readsAfterCancel = bus.readCalls;
  OperationToken blockedToken;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.startRead(request, 201, 401, blockedToken).code));

  const uint32_t waitStartMs = 201U;
  PollResult poll = dev.poll(waitStartMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesAfterCancel, readsAfterCancel);
  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128) - 1U, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesAfterCancel, readsAfterCancel);
  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128), 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CANCELLED),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesAfterCancel, readsAfterCancel);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_TRUE(result.hardwareStateUncertain);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CONFIG_UNKNOWN),
                          static_cast<uint8_t>(dev.startRead(request, 220, 420, blockedToken).code));
}

void test_owner_safe_cancel_after_ambiguous_start_preserves_transport_error() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  bus.failWriteOnCall = 1;
  bus.applyFailedWrite = true;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "ambiguous start timeout", -201);
  ChannelRequest request;
  request.mux = Mux::AIN3_GND;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200, 400, token).inProgress());

  PollResult poll = dev.poll(200, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-201, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(poll.operationState));
  TEST_ASSERT_EQUAL_UINT16(bus.lastWriteValue, bus.reg[cmd::REG_CONFIG]);
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CancelDisposition::RECONCILIATION_REQUIRED),
      static_cast<uint8_t>(dev.cancelActiveOperation()));

  const uint32_t writesBeforeWait = bus.writeCalls;
  const uint32_t readsBeforeWait = bus.readCalls;
  const uint32_t waitStartMs = 201U;
  poll = dev.poll(waitStartMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeWait, readsBeforeWait);
  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_128), 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-201, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeWait, readsBeforeWait);
  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(result.status.code));
  TEST_ASSERT_EQUAL_INT32(-201, result.status.detail);
}

void test_owner_safe_ambiguous_delayed_start_uses_post_callback_quiet_interval() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  DeviceProfile profile = makeDeviceProfile();
  profile.dataRate = DataRate::SPS_860;
  initializeOwnerSafe(dev, bus, profile);
  resetIoCounters(bus);
  bus.nowMs = 200U;
  bus.writeAdvanceMs = 9U;
  bus.failWriteOnCall = 1;
  bus.applyFailedWrite = true;
  bus.failWriteStatus =
      Status::Error(Err::I2C_TIMEOUT, "delayed ambiguous start", -202);

  ChannelRequest request;
  request.mux = Mux::AIN2_GND;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200U, 400U, token).inProgress());

  PollResult poll = dev.poll(200U, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT32(209U, bus.nowMs);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::RECONCILING),
                          static_cast<uint8_t>(poll.operationState));
  const uint32_t writesBeforeWait = bus.writeCalls;
  const uint32_t readsBeforeWait = bus.readCalls;

  const uint32_t waitStartMs = bus.nowMs;
  poll = dev.poll(waitStartMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_860) - 1U, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeWait, readsBeforeWait);

  poll = dev.poll(waitStartMs + ownerConversionTimeMs(DataRate::SPS_860), 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_INT32(-202, poll.status.detail);
  TEST_ASSERT_EQUAL_UINT8(0U, poll.instructionsUsed);
  assertNoIoSince(bus, writesBeforeWait, readsBeforeWait);

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_FALSE(result.sampleValid);
  TEST_ASSERT_TRUE(result.hardwareStateUncertain);
}

void test_owner_safe_cancel_after_ready_verification_discards_sample_without_dirtying() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  bus.reg[cmd::REG_CONVERSION] = 0x4567;
  ChannelRequest request;
  request.channelId = 19;
  request.mux = Mux::AIN1_GND;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200, 400, token).inProgress());
  TEST_ASSERT_FALSE(dev.poll(200, 1).done);
  const uint32_t readyMs = 200U + ownerConversionTimeMs(DataRate::SPS_128);
  PollResult poll = dev.poll(readyMs, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(JobState::SINGLE_SHOT_READ_CONVERSION),
                          static_cast<uint8_t>(poll.state));
  TEST_ASSERT_EQUAL_UINT32(1U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(CancelDisposition::CANCELLED_AFTER_EFFECT),
      static_cast<uint8_t>(dev.cancelActiveOperation()));
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(dev.configurationState()));
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::CANCELLED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_FALSE(result.sampleValid);
  TEST_ASSERT_FALSE(result.hardwareStateUncertain);

  OperationToken restartToken;
  TEST_ASSERT_TRUE(dev.startRead(request, readyMs + 1U, readyMs + 201U,
                                 restartToken).inProgress());
}

void test_owner_safe_profile_cancel_after_each_pending_stage_marks_effects_unknown() {
  for (uint8_t completedTransfers = 0; completedTransfers < 6U;
       ++completedTransfers) {
    FakeBus bus;
    ADS1115::ADS1115 dev;
    initializeOwnerSafe(dev, bus, makeDeviceProfile());
    resetIoCounters(bus);
    DeviceProfile candidate = makeDeviceProfile();
    candidate.defaultGain = Gain::FSR_0_512V;
    OperationToken token;
    TEST_ASSERT_TRUE(dev.startApplyProfile(candidate, 200, 1000, token).inProgress());

    for (uint8_t step = 0; step < completedTransfers; ++step) {
      const PollResult poll = dev.poll(200, 1);
      TEST_ASSERT_FALSE(poll.done);
      TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
    }
    const uint32_t writesBeforeCancel = bus.writeCalls;
    const uint32_t readsBeforeCancel = bus.readCalls;
    const bool writeEffectPossible = completedTransfers >= 1U;
    const CancelDisposition disposition = dev.cancelActiveOperation();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(writeEffectPossible
                                 ? CancelDisposition::CANCELLED_AFTER_EFFECT
                                 : CancelDisposition::CANCELLED_BEFORE_EFFECT),
        static_cast<uint8_t>(disposition));
    assertNoIoSince(bus, writesBeforeCancel, readsBeforeCancel);

    OperationResult result;
    TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::CANCELLED),
                            static_cast<uint8_t>(result.state));
    TEST_ASSERT_EQUAL(writeEffectPossible, result.hardwareStateUncertain);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(writeEffectPossible
                                 ? ConfigurationState::UNKNOWN
                                 : ConfigurationState::VERIFIED),
        static_cast<uint8_t>(dev.configurationState()));
    TEST_ASSERT_EQUAL(writeEffectPossible, dev.hardwareConfigDirty());
    TEST_ASSERT_EQUAL_UINT32(1U, dev.configurationGeneration());
  }
}

void test_owner_safe_apply_profile_success_commits_candidate_snapshot_and_generation() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  const DeviceProfile original = makeDeviceProfile();
  initializeOwnerSafe(dev, bus, original);
  resetIoCounters(bus);

  DeviceProfile candidate = original;
  candidate.defaultMux = Mux::AIN3_GND;
  candidate.defaultGain = Gain::FSR_0_512V;
  candidate.dataRate = DataRate::SPS_475;
  candidate.comparator.use = ComparatorUse::THRESHOLD;
  candidate.comparator.mode = ComparatorMode::WINDOW;
  candidate.comparator.polarity = ComparatorPolarity::ACTIVE_HIGH;
  candidate.comparator.latch = ComparatorLatch::LATCHING;
  candidate.comparator.queue = ComparatorQueue::ASSERT_4;
  candidate.comparator.lowThreshold = -1234;
  candidate.comparator.highThreshold = 2345;

  OperationToken token;
  const uint32_t writesBeforeStart = bus.writeCalls;
  const uint32_t readsBeforeStart = bus.readCalls;
  TEST_ASSERT_TRUE(dev.startApplyProfile(candidate, 200U, 1000U, token).inProgress());
  TEST_ASSERT_TRUE(token.valid());
  assertNoIoSince(bus, writesBeforeStart, readsBeforeStart);

  AppliedProfileSnapshot applying;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(applying).ok());
  assertDeviceProfilesEqual(original, applying.profile);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::APPLYING),
                          static_cast<uint8_t>(applying.state));
  TEST_ASSERT_EQUAL_UINT32(1U, applying.generation);

  PollResult poll;
  for (uint8_t step = 0; step < 6U; ++step) {
    poll = dev.poll(200U, 1U);
    TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
    TEST_ASSERT_EQUAL(step == 5U, poll.done);
  }
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT32(3U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(3U, bus.readCalls);

  AppliedProfileSnapshot applied;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(applied).ok());
  assertDeviceProfilesEqual(candidate, applied.profile);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(applied.state));
  TEST_ASSERT_EQUAL_UINT32(2U, applied.generation);
  TEST_ASSERT_EQUAL_UINT32(2U, dev.configurationGeneration());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT32(token.value, result.token.value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::APPLY_PROFILE),
                          static_cast<uint8_t>(result.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::SUCCEEDED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_TRUE(result.status.ok());
  TEST_ASSERT_FALSE(result.sampleValid);
  TEST_ASSERT_FALSE(result.hardwareStateUncertain);
}

void test_owner_safe_tokened_shutdown_verifies_idle_and_publishes_result() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  DeviceProfile profile = makeDeviceProfile();
  profile.mode = Mode::CONTINUOUS;
  initializeOwnerSafe(dev, bus, profile);
  resetIoCounters(bus);

  OperationToken token;
  TEST_ASSERT_TRUE(dev.startShutdown(200U, 400U, token).inProgress());
  TEST_ASSERT_TRUE(token.valid());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);

  PollResult poll = dev.poll(200U, 1U);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);
  poll = dev.poll(200U, 1U);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);

  AppliedProfileSnapshot applied;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(applied).ok());
  profile.mode = Mode::SINGLE_SHOT;
  assertDeviceProfilesEqual(profile, applied.profile);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(applied.state));
  TEST_ASSERT_EQUAL_UINT32(2U, applied.generation);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Mode::SINGLE_SHOT),
                          static_cast<uint8_t>(dev.getConfig().mode));
  TEST_ASSERT_TRUE(dev.isInitialized());

  OperationResult result;
  TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
  TEST_ASSERT_EQUAL_UINT32(token.value, result.token.value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::SHUTDOWN),
                          static_cast<uint8_t>(result.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::SUCCEEDED),
                          static_cast<uint8_t>(result.state));
  TEST_ASSERT_TRUE(result.status.ok());
  TEST_ASSERT_FALSE(result.sampleValid);
  TEST_ASSERT_FALSE(result.hardwareStateUncertain);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::RESULT_NOT_AVAILABLE),
                          static_cast<uint8_t>(dev.takeResult(token, result).code));
}

void test_owner_safe_recover_initializes_bound_driver_after_failed_initialize() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  const DeviceProfile profile = makeDeviceProfile();
  TEST_ASSERT_TRUE(dev.bind(makeDriverConfig(bus), profile).ok());

  bus.failReadOnCall = 1U;
  bus.failReadStatus =
      Status::Error(Err::I2C_NACK_ADDR, "initial owner probe failed", -48);
  OperationToken initializeToken;
  TEST_ASSERT_TRUE(dev.startInitialize(100U, 1000U, initializeToken).inProgress());
  PollResult poll = dev.poll(100U, 1U);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::DEVICE_NOT_FOUND),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_TRUE(dev.isBound());
  TEST_ASSERT_FALSE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::UNCONFIGURED),
                          static_cast<uint8_t>(dev.configurationState()));
  TEST_ASSERT_EQUAL_UINT32(0U, dev.configurationGeneration());
  OperationResult initializeResult;
  TEST_ASSERT_TRUE(dev.takeResult(initializeToken, initializeResult).ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::INITIALIZE),
                          static_cast<uint8_t>(initializeResult.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::FAILED),
                          static_cast<uint8_t>(initializeResult.state));

  resetIoCounters(bus);
  OperationToken recoverToken;
  TEST_ASSERT_TRUE(dev.startRecover(200U, 1200U, recoverToken).inProgress());
  TEST_ASSERT_TRUE(recoverToken.valid());
  TEST_ASSERT_EQUAL_UINT32(0U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(0U, bus.readCalls);
  for (uint8_t step = 0; step < 7U; ++step) {
    poll = dev.poll(200U, 1U);
    TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
    TEST_ASSERT_EQUAL(step == 6U, poll.done);
  }
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT32(3U, bus.writeCalls);
  TEST_ASSERT_EQUAL_UINT32(4U, bus.readCalls);
  TEST_ASSERT_TRUE(dev.isInitialized());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(ConfigurationState::VERIFIED),
                          static_cast<uint8_t>(dev.configurationState()));
  TEST_ASSERT_EQUAL_UINT32(1U, dev.configurationGeneration());
  AppliedProfileSnapshot applied;
  TEST_ASSERT_TRUE(dev.getAppliedProfile(applied).ok());
  assertDeviceProfilesEqual(profile, applied.profile);

  OperationResult recoverResult;
  TEST_ASSERT_TRUE(dev.takeResult(recoverToken, recoverResult).ok());
  TEST_ASSERT_EQUAL_UINT32(recoverToken.value, recoverResult.token.value);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationKind::RECOVER),
                          static_cast<uint8_t>(recoverResult.kind));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperationState::SUCCEEDED),
                          static_cast<uint8_t>(recoverResult.state));
  TEST_ASSERT_TRUE(recoverResult.status.ok());
}

void test_owner_safe_sample_result_sets_positive_and_negative_code_limit_flags() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  ChannelRequest request;

  const int16_t rawCodes[] = {INT16_MAX, INT16_MIN};
  const SampleFlag expectedFlags[] = {SampleFlag::AT_POSITIVE_CODE_LIMIT,
                                      SampleFlag::AT_NEGATIVE_CODE_LIMIT};
  const SampleFlag absentFlags[] = {SampleFlag::AT_NEGATIVE_CODE_LIMIT,
                                    SampleFlag::AT_POSITIVE_CODE_LIMIT};
  const int32_t expectedMicrovolts[] = {2047938, -2048000};
  for (uint8_t index = 0; index < 2U; ++index) {
    bus.reg[cmd::REG_CONVERSION] = static_cast<uint16_t>(rawCodes[index]);
    const uint32_t startMs = 200U + static_cast<uint32_t>(index) * 100U;
    OperationToken token;
    TEST_ASSERT_TRUE(
        dev.startRead(request, startMs, startMs + 200U, token).inProgress());
    TEST_ASSERT_FALSE(dev.poll(startMs, 1U).done);
    const uint32_t readyMs = startMs + ownerConversionTimeMs(DataRate::SPS_128);
    TEST_ASSERT_FALSE(dev.poll(readyMs, 1U).done);
    const PollResult poll = dev.poll(readyMs, 1U);
    TEST_ASSERT_TRUE(poll.done);
    TEST_ASSERT_TRUE(poll.status.ok());

    OperationResult result;
    TEST_ASSERT_TRUE(dev.takeResult(token, result).ok());
    TEST_ASSERT_TRUE(result.sampleValid);
    TEST_ASSERT_EQUAL_INT16(rawCodes[index], result.sample.rawCode);
    TEST_ASSERT_EQUAL_INT32(expectedMicrovolts[index], result.sample.microvolts);
    TEST_ASSERT_TRUE(
        (result.sample.flags & static_cast<uint16_t>(SampleFlag::CONFIG_VERIFIED)) != 0U);
    TEST_ASSERT_TRUE(
        (result.sample.flags & static_cast<uint16_t>(expectedFlags[index])) != 0U);
    TEST_ASSERT_TRUE(
        (result.sample.flags & static_cast<uint16_t>(absentFlags[index])) == 0U);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(index) + 1U,
                             result.sample.sequence);
  }
}

void test_owner_safe_dirty_configuration_blocks_typed_read_until_verified_recover() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  TEST_ASSERT_TRUE(dev.writeRegister16(cmd::REG_CONFIG,
                                       static_cast<uint16_t>(cmd::CONFIG_DEFAULT ^ cmd::MASK_DR)).ok());
  TEST_ASSERT_TRUE(dev.hardwareConfigDirty());
  const uint32_t writesAfterRaw = bus.writeCalls;
  const uint32_t readsAfterRaw = bus.readCalls;
  ChannelRequest request;
  OperationToken token;
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::CONFIG_UNKNOWN),
                          static_cast<uint8_t>(dev.startRead(request, 200, 400, token).code));
  assertNoIoSince(bus, writesAfterRaw, readsAfterRaw);

  TEST_ASSERT_TRUE(dev.startRecover(200, 1000, token).inProgress());
  PollResult poll;
  for (uint8_t step = 0; step < 7U; ++step) {
    poll = dev.poll(200, 1);
  }
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  OperationResult recoverResult;
  TEST_ASSERT_TRUE(dev.takeResult(token, recoverResult).ok());
  TEST_ASSERT_FALSE(dev.hardwareConfigDirty());
  TEST_ASSERT_TRUE(dev.startRead(request, 201, 401, token).inProgress());
}

void test_owner_safe_direct_mutations_are_blocked_during_conversion() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  resetIoCounters(bus);
  ChannelRequest request;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startRead(request, 200, 400, token).inProgress());
  TEST_ASSERT_FALSE(dev.poll(200, 1).done);
  const uint32_t writesAfterStart = bus.writeCalls;
  const uint32_t readsAfterStart = bus.readCalls;

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.setMux(Mux::AIN1_GND).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.setGain(Gain::FSR_0_512V).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.setDataRate(DataRate::SPS_860).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.setMode(Mode::CONTINUOUS).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.writeConfig(cmd::CONFIG_DEFAULT).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.setThresholds(-1, 1).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.setComparatorMode(ComparatorMode::WINDOW).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.setComparatorPolarity(ComparatorPolarity::ACTIVE_HIGH).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.setComparatorLatch(ComparatorLatch::LATCHING).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.setComparatorQueue(ComparatorQueue::ASSERT_1).code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.enableConversionReadyPin().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.disableComparator().code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::BUSY),
                          static_cast<uint8_t>(dev.shutdown().code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.writeRegister16(cmd::REG_CONFIG, cmd::CONFIG_DEFAULT).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.writeRegister16(cmd::REG_LO_THRESH, 0).code));
  DeviceProfile candidate = makeDeviceProfile();
  OperationToken blockedToken;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.startApplyProfile(candidate, 201, 401, blockedToken).code));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(Err::BUSY),
      static_cast<uint8_t>(dev.startShutdown(201, 401, blockedToken).code));
  assertNoIoSince(bus, writesAfterStart, readsAfterStart);
}

void test_owner_safe_health_is_passive_and_recovery_transport_is_not_offline_gated() {
  FakeBus bus;
  ADS1115::ADS1115 dev;
  initializeOwnerSafe(dev, bus, makeDeviceProfile());
  dev._config.offlineThreshold = 1;
  resetIoCounters(bus);
  DeviceProfile candidate = makeDeviceProfile();
  candidate.defaultGain = Gain::FSR_0_512V;
  OperationToken token;
  TEST_ASSERT_TRUE(dev.startApplyProfile(candidate, 200, 1000, token).inProgress());
  bus.failWriteOnCall = 1;
  bus.failWriteStatus = Status::Error(Err::I2C_TIMEOUT, "owner apply timeout", -202);
  PollResult poll = dev.poll(200, 1);
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Err::I2C_TIMEOUT),
                          static_cast<uint8_t>(poll.status.code));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::OFFLINE),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(200U, dev.lastErrorMs());
  OperationResult failed;
  TEST_ASSERT_TRUE(dev.takeResult(token, failed).ok());

  resetIoCounters(bus);
  TEST_ASSERT_TRUE(dev.startRecover(201, 1000, token).inProgress());
  poll = dev.poll(201, 1);
  TEST_ASSERT_FALSE(poll.done);
  TEST_ASSERT_EQUAL_UINT8(1U, poll.instructionsUsed);
  TEST_ASSERT_EQUAL_UINT32(1U, bus.readCalls);
  for (uint8_t step = 1; step < 7U; ++step) {
    poll = dev.poll(201, 1);
  }
  TEST_ASSERT_TRUE(poll.done);
  TEST_ASSERT_TRUE(poll.status.ok());
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DriverState::READY),
                          static_cast<uint8_t>(dev.state()));
  TEST_ASSERT_EQUAL_UINT32(201U, dev.lastOkMs());
  OperationResult recovered;
  TEST_ASSERT_TRUE(dev.takeResult(token, recovered).ok());
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
  RUN_TEST(test_begin_rejects_i2c_address_boundaries_without_bus_access);
  RUN_TEST(test_begin_rejects_invalid_enum_values_without_bus_access);
  RUN_TEST(test_begin_rejects_invalid_alert_ready_gpio_config_without_bus_access);
  RUN_TEST(test_invalid_begin_preserves_existing_valid_binding);
  RUN_TEST(test_failed_begin_probe_keeps_valid_candidate_binding_for_retry);
  RUN_TEST(test_begin_normalizes_offline_threshold_on_stored_copy);
  RUN_TEST(test_begin_success_sets_ready_and_counters);
  RUN_TEST(test_begin_strict_readback_success_masks_config_os_bit);
  RUN_TEST(test_begin_strict_readback_mismatch_fails_without_initializing_and_preserves_dirty);
  RUN_TEST(test_begin_strict_low_threshold_readback_mismatch_reports_observed_detail);
  RUN_TEST(test_begin_strict_high_threshold_readback_mismatch_reports_observed_detail);
  RUN_TEST(test_begin_failure_after_first_apply_write_preserves_dirty_diagnostic);
  RUN_TEST(test_begin_partial_write_failure_preserves_dirty_address);
  RUN_TEST(test_begin_failure_after_second_apply_write_preserves_dirty_diagnostic);
  RUN_TEST(test_begin_failure_on_third_apply_write_preserves_original_status_and_dirty);
  RUN_TEST(test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic);
  RUN_TEST(test_successful_begin_clears_prior_failed_begin_dirty_diagnostic);
  RUN_TEST(test_failed_begin_dirty_survives_later_probe_failure);
  RUN_TEST(test_recover_strict_readback_success_clears_dirty);
  RUN_TEST(test_recover_success_clears_dirty_address);
  RUN_TEST(test_recover_strict_readback_mismatch_keeps_dirty_and_preserves_error);
  RUN_TEST(test_recover_strict_low_threshold_mismatch_keeps_dirty_and_preserves_error);
  RUN_TEST(test_recover_strict_high_threshold_mismatch_keeps_dirty_and_preserves_error);
  RUN_TEST(test_recover_strict_readback_transport_failure_keeps_dirty_and_preserves_error);
  RUN_TEST(test_probe_failure_does_not_update_health);
  RUN_TEST(test_probe_before_begin_returns_not_initialized_without_bus_access);
  RUN_TEST(test_probe_after_end_returns_not_initialized_without_bus_access);
  RUN_TEST(test_probe_maps_i2c_nack_addr_to_device_not_found);
  RUN_TEST(test_probe_preserves_distinct_i2c_errors);
  RUN_TEST(test_recover_probe_failure_updates_passive_health_diagnostics);
  RUN_TEST(test_recover_success_after_tracked_probe_failure_returns_ready);
  RUN_TEST(test_tracked_failure_reaches_passive_offline_diagnostic_threshold);
  RUN_TEST(test_passive_offline_diagnostic_does_not_gate_authorized_read);
  RUN_TEST(test_pending_conversion_busy_precedes_passive_offline_diagnostic_without_bus_access);
  RUN_TEST(test_passive_offline_diagnostic_allows_cached_readiness_without_bus_access);
  RUN_TEST(test_passive_offline_diagnostic_allows_due_conversion_service);
  RUN_TEST(test_partial_recover_success_resets_offline_then_failure_degrades);
  RUN_TEST(test_tracked_read_preserves_distinct_i2c_errors_and_updates_health);
  RUN_TEST(test_tracked_write_preserves_distinct_i2c_errors_and_updates_health);
  RUN_TEST(test_start_conversion_in_continuous_mode_returns_unsupported_operation);
  RUN_TEST(test_start_conversion_while_single_shot_pending_returns_busy_without_bus_access);
  RUN_TEST(test_single_shot_timing_wraparound_reaches_ready);
  RUN_TEST(test_single_shot_raw_read_consumes_ready_before_voltage_read);
  RUN_TEST(test_read_conversion_ready_propagates_i2c_failure);
  RUN_TEST(test_conversion_ready_convenience_returns_false_on_failure);
  RUN_TEST(test_conversion_ready_status_alias_preserves_transport_error);
  RUN_TEST(test_service_returns_ready_poll_failure_and_updates_health);
  RUN_TEST(test_tick_discards_status_but_updates_health_on_ready_poll_failure);
  RUN_TEST(test_no_clock_direct_readiness_waits_for_service_timebase);
  RUN_TEST(test_no_clock_alert_ready_pin_uses_external_service_timebase);
  RUN_TEST(test_no_clock_health_timestamps_are_marked_unavailable);
  RUN_TEST(test_read_raw_propagates_ready_poll_failure);
  RUN_TEST(test_read_raw_reconstructs_signed_conversion_register);
  RUN_TEST(test_continuous_readiness_waits_for_data_rate_interval);
  RUN_TEST(test_tick_marks_continuous_ready_without_config_poll_after_interval);
  RUN_TEST(test_continuous_read_raw_returns_latest_without_fresh_wait);
  RUN_TEST(test_read_blocking_continuous_is_rejected_without_bus_access);
  RUN_TEST(test_continuous_blocking_rejection_precedes_diagnostic_transport_error);
  RUN_TEST(test_single_shot_elapsed_os_busy_remains_not_ready);
  RUN_TEST(test_alert_ready_pin_path_does_not_poll_config_register);
  RUN_TEST(test_poll_single_shot_max_one_wait_gate_and_raw_result);
  RUN_TEST(test_poll_single_shot_budget_two_reads_ready_and_conversion);
  RUN_TEST(test_poll_single_shot_repeated_zero_budget_does_not_advance_or_touch_bus);
  RUN_TEST(test_poll_single_shot_large_budget_is_bounded_and_poll_after_complete_is_stable);
  RUN_TEST(test_poll_single_shot_ready_transport_failure_propagates);
  RUN_TEST(test_start_apply_config_job_in_continuous_mode_is_supported);
  RUN_TEST(test_start_apply_config_job_rejects_active_single_shot_conversion);
  RUN_TEST(test_poll_apply_config_continuous_mode_finishes_with_continuous_timing_state);
  RUN_TEST(test_poll_apply_config_budget_and_strict_readback);
  RUN_TEST(test_poll_apply_config_zero_budget_and_large_budget_clamp);
  RUN_TEST(test_compatibility_staged_jobs_restart_after_terminal_result_ack);
  RUN_TEST(test_wrong_job_poller_returns_busy_without_advancing_or_touching_bus);
  RUN_TEST(test_cancel_job_publishes_terminal_result_before_restart);
  RUN_TEST(test_active_job_service_advances_one_step_and_other_public_i2c_apis_stay_blocked);
  RUN_TEST(test_active_job_preserves_invalid_param_precedence_without_bus_access);
  RUN_TEST(test_end_while_job_active_clears_job_and_later_poll_is_not_initialized);
  RUN_TEST(test_start_single_shot_rejects_invalid_mux_without_bus_access);
  RUN_TEST(test_poll_single_shot_uncertain_write_failure_reconciles_bus_silently);
  RUN_TEST(test_poll_single_shot_first_write_address_nack_keeps_clean);
  RUN_TEST(test_poll_single_shot_first_write_timeout_requires_idle_reconciliation);
  RUN_TEST(test_poll_single_shot_conversion_read_failure_updates_health_without_dirty);
  RUN_TEST(test_poll_apply_config_first_write_address_nack_keeps_clean);
  RUN_TEST(test_poll_apply_config_readback_mismatch_keeps_dirty_and_preserves_status);
  RUN_TEST(test_poll_apply_config_partial_failure_marks_dirty);
  RUN_TEST(test_poll_apply_config_first_write_failure_marks_dirty);
  RUN_TEST(test_config_setters_write_expected_config_bits);
  RUN_TEST(test_public_setters_reject_invalid_enum_values_without_bus_access);
  RUN_TEST(test_write_config_normalizes_datasheet_pga_aliases_for_0_256v);
  RUN_TEST(test_threshold_writes_commit_cache_after_both_registers_succeed);
  RUN_TEST(test_thresholds_accept_and_reconstruct_int16_boundaries);
  RUN_TEST(test_threshold_diagnostic_read_invalidates_full_profile_trust);
  RUN_TEST(test_lsb_voltage_for_all_gain_ranges);
  RUN_TEST(test_raw_to_voltage_for_all_gain_ranges_and_representative_codes);
  RUN_TEST(test_set_gain_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_config_only_setters_roll_back_each_cached_field_on_write_failure);
  RUN_TEST(test_start_conversion_with_mux_rolls_back_cache_but_remains_active_when_ambiguous);
  RUN_TEST(test_set_thresholds_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_set_thresholds_second_write_failure_preserves_cache_and_dirty_reason);
  RUN_TEST(test_comparator_setter_does_not_commit_cache_on_write_failure);
  RUN_TEST(test_comparator_setters_roll_back_each_cached_field_on_write_failure);
  RUN_TEST(test_disable_comparator_rolls_back_queue_on_write_failure);
  RUN_TEST(test_enable_conversion_ready_pin_rolls_back_cache_on_write_failure);
  RUN_TEST(test_enable_conversion_ready_pin_partial_failure_rolls_back_cache_and_dirty_reason);
  RUN_TEST(test_apply_config_failure_on_first_write_marks_hardware_dirty);
  RUN_TEST(test_apply_config_failure_on_second_write_marks_hardware_dirty);
  RUN_TEST(test_apply_config_failure_on_third_write_marks_hardware_dirty);
  RUN_TEST(test_set_thresholds_second_write_failure_marks_hardware_dirty);
  RUN_TEST(test_set_thresholds_first_write_failure_marks_hardware_dirty);
  RUN_TEST(test_enable_conversion_ready_pin_partial_failure_marks_hardware_dirty);
  RUN_TEST(test_recover_success_clears_hardware_dirty_after_full_resync);
  RUN_TEST(test_successful_config_only_setter_does_not_clear_prior_dirty);
  RUN_TEST(test_failed_config_only_setter_preserves_prior_dirty_reason);
  RUN_TEST(test_failed_config_only_setter_marks_dirty_when_clean);
  RUN_TEST(test_failed_config_only_setter_address_nack_keeps_clean);
  RUN_TEST(test_failed_write_config_marks_dirty_when_clean);
  RUN_TEST(test_failed_shutdown_marks_dirty_when_clean);
  RUN_TEST(test_failed_start_conversion_mux_override_marks_dirty_when_clean);
  RUN_TEST(test_failed_config_and_comparator_setters_preserve_prior_dirty_reason);
  RUN_TEST(test_invalid_raw_register_is_rejected_without_bus_access);
  RUN_TEST(test_write_conversion_register_is_rejected_as_read_only);
  RUN_TEST(test_raw_config_write_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_raw_low_threshold_write_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_raw_high_threshold_write_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_write_register_alias_marks_hardware_config_dirty_without_cache_commit);
  RUN_TEST(test_failed_raw_write_marks_hardware_config_dirty_with_transport_error);
  RUN_TEST(test_passive_offline_diagnostic_allows_authorized_raw_write);
  RUN_TEST(test_recover_success_clears_raw_register_dirty_after_full_resync);
  RUN_TEST(test_recover_raw_dirty_requires_verified_readback_before_clear);
  RUN_TEST(test_failed_recover_probe_preserves_raw_register_dirty_reason);
  RUN_TEST(test_failed_recover_partial_apply_keeps_raw_register_dirty_visible);
  RUN_TEST(test_read_blocking_stalled_clock_enters_bus_silent_reconciliation);
  RUN_TEST(test_read_blocking_rejects_invalid_timeout_without_starting_conversion);
  RUN_TEST(test_read_blocking_success_uses_cooperative_yield_cadence);
  RUN_TEST(test_read_blocking_tolerates_repeated_same_tick_before_clock_advances);
  RUN_TEST(test_read_blocking_rejects_joining_existing_direct_conversion_without_bus_access);
  RUN_TEST(test_read_blocking_times_out_with_advancing_clock_while_os_busy);
  RUN_TEST(test_read_blocking_propagates_ready_poll_transport_error);
  RUN_TEST(test_read_blocking_requires_now_ms_without_starting_conversion);
  RUN_TEST(test_read_blocking_voltage_requires_now_ms_without_starting_conversion);
  RUN_TEST(test_scaled_reads_report_lifecycle_and_continuous_mode_preconditions);
  RUN_TEST(test_raw_transport_rejects_invalid_buffers);
  RUN_TEST(test_bus_silent_end_then_register_access_does_not_touch_bus);
  RUN_TEST(test_shutdown_success_writes_single_shot_mode_and_keeps_initialized);
  RUN_TEST(test_shutdown_returns_original_transport_error);
  RUN_TEST(test_shutdown_proceeds_when_passive_health_is_offline);
  RUN_TEST(test_end_ignores_shutdown_write_failure_and_uninitializes);
  RUN_TEST(test_end_while_offline_does_not_touch_bus);
  RUN_TEST(test_owner_safe_pure_helpers_cover_boundaries_and_exact_units);
  RUN_TEST(test_owner_safe_profile_validation_boundaries);
  RUN_TEST(test_owner_safe_bind_and_unbind_are_zero_i2c);
  RUN_TEST(test_owner_safe_initialize_uses_one_transfer_polls_and_commits_generation_after_readback);
  RUN_TEST(test_owner_safe_initialize_readback_failure_keeps_generation_zero_and_unknown);
  RUN_TEST(test_owner_safe_poll_clamps_callback_timeout_to_deadline_remaining);
  RUN_TEST(test_owner_safe_multi_callback_poll_partitions_remaining_deadline_budget);
  RUN_TEST(test_owner_safe_reinitialize_and_rebind_reject_active_direct_conversion);
  RUN_TEST(test_owner_safe_initialize_cancel_after_each_pending_stage_is_bus_silent);
  RUN_TEST(test_owner_safe_tokened_read_publishes_atomic_sample_exactly_once);
  RUN_TEST(test_owner_safe_deadline_is_wrap_safe_and_reconciles_without_i2c);
  RUN_TEST(test_owner_safe_cancel_before_io_is_terminal_and_bus_silent);
  RUN_TEST(test_owner_safe_cancel_after_confirmed_start_waits_bus_silently_and_blocks_restart);
  RUN_TEST(test_owner_safe_cancel_after_ambiguous_start_preserves_transport_error);
  RUN_TEST(test_owner_safe_ambiguous_delayed_start_uses_post_callback_quiet_interval);
  RUN_TEST(test_owner_safe_cancel_after_ready_verification_discards_sample_without_dirtying);
  RUN_TEST(test_owner_safe_profile_cancel_after_each_pending_stage_marks_effects_unknown);
  RUN_TEST(test_owner_safe_apply_profile_success_commits_candidate_snapshot_and_generation);
  RUN_TEST(test_owner_safe_tokened_shutdown_verifies_idle_and_publishes_result);
  RUN_TEST(test_owner_safe_recover_initializes_bound_driver_after_failed_initialize);
  RUN_TEST(test_owner_safe_sample_result_sets_positive_and_negative_code_limit_flags);
  RUN_TEST(test_owner_safe_dirty_configuration_blocks_typed_read_until_verified_recover);
  RUN_TEST(test_owner_safe_direct_mutations_are_blocked_during_conversion);
  RUN_TEST(test_owner_safe_health_is_passive_and_recovery_transport_is_not_offline_gated);
  return UNITY_END();
}
