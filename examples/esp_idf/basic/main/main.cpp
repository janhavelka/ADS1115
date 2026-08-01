/**
 * @file main.cpp
 * @brief Native ESP-IDF ADS1115 bring-up CLI.
 *
 * This example intentionally does not include Arduino headers, Arduino CLI
 * sources, or Arduino compatibility facades. It keeps command parity with the
 * Arduino example using IDF-native entry, timing, GPIO, delays, and I2C.
 */

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ADS1115/ADS1115.h"
#include "Ads1115IdfI2cTransport.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

static constexpr int I2C_SDA = 8;
static constexpr int I2C_SCL = 9;
static constexpr uint32_t I2C_FREQ_HZ = 400000U;
static constexpr uint16_t I2C_TIMEOUT_MS = 50U;
static constexpr uint8_t DEFAULT_ADS1115_ADDRESS = 0x48U;
static constexpr int ALERT_RDY_PIN = -1;
static constexpr size_t MAX_LINE_LEN = 128U;
static constexpr uint32_t STRESS_PROGRESS_UPDATES = 10U;
static constexpr uint32_t DIAGNOSTIC_JOB_TIMEOUT_MS = 5000U;

static constexpr const char* COLOR_RESET = "\033[0m";
static constexpr const char* COLOR_RED = "\033[31m";
static constexpr const char* COLOR_GREEN = "\033[32m";
static constexpr const char* COLOR_YELLOW = "\033[33m";
static constexpr const char* COLOR_CYAN = "\033[36m";

ADS1115::ADS1115 device;
bool verboseMode = false;
uint8_t activeI2cAddress = DEFAULT_ADS1115_ADDRESS;
uint8_t requestedI2cAddress = DEFAULT_ADS1115_ADDRESS;
ADS1115::Status lastAddressSelectionStatus = ADS1115::Status::Ok();

uint32_t nowMs() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

void sleepMs(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms == 0U ? 1U : ms));
}

bool readAlertRdyPin(int pin, void*) {
  return gpio_get_level(static_cast<gpio_num_t>(pin)) != 0;
}

void initAlertRdyPin() {
  if constexpr (ALERT_RDY_PIN < 0) {
    return;
  } else {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << static_cast<uint32_t>(ALERT_RDY_PIN);
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    (void)gpio_config(&cfg);
  }
}

void trimInPlace(char* text) {
  if (text == nullptr) {
    return;
  }
  char* start = text;
  while (*start != '\0' && std::isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }
  if (start != text) {
    std::memmove(text, start, std::strlen(start) + 1U);
  }
  size_t len = std::strlen(text);
  while (len > 0U && std::isspace(static_cast<unsigned char>(text[len - 1U]))) {
    text[--len] = '\0';
  }
}

bool startsWith(const char* text, const char* prefix) {
  return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
}

const char* argAfter(const char* text, const char* prefix) {
  if (!startsWith(text, prefix)) {
    return nullptr;
  }
  return text + std::strlen(prefix);
}

bool parseI32(const char* token, int32_t& out) {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const long value = std::strtol(token, &end, 0);
  if (end == token || *end != '\0') {
    return false;
  }
  out = static_cast<int32_t>(value);
  return true;
}

bool parseU32(const char* token, uint32_t& out) {
  if (token == nullptr || token[0] == '\0') {
    return false;
  }
  char* end = nullptr;
  const unsigned long value = std::strtoul(token, &end, 0);
  if (end == token || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseBool01(const char* token, bool& out) {
  int32_t value = 0;
  if (!parseI32(token, value) || (value != 0 && value != 1)) {
    return false;
  }
  out = (value != 0);
  return true;
}

bool isValidAds1115Address(uint32_t address) {
  return address >= 0x48U && address <= 0x4BU;
}

const char* errToStr(ADS1115::Err err) {
  using ADS1115::Err;
  switch (err) {
    case Err::OK: return "OK";
    case Err::NOT_INITIALIZED: return "NOT_INITIALIZED";
    case Err::INVALID_CONFIG: return "INVALID_CONFIG";
    case Err::I2C_ERROR: return "I2C_ERROR";
    case Err::TIMEOUT: return "TIMEOUT";
    case Err::INVALID_PARAM: return "INVALID_PARAM";
    case Err::DEVICE_NOT_FOUND: return "DEVICE_NOT_FOUND";
    case Err::CONVERSION_NOT_READY: return "CONVERSION_NOT_READY";
    case Err::BUSY: return "BUSY";
    case Err::IN_PROGRESS: return "IN_PROGRESS";
    case Err::I2C_NACK_ADDR: return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA: return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT: return "I2C_TIMEOUT";
    case Err::I2C_BUS: return "I2C_BUS";
    case Err::OFFLINE: return "OFFLINE";
    case Err::UNSUPPORTED_OPERATION: return "UNSUPPORTED_OPERATION";
    case Err::READBACK_MISMATCH: return "READBACK_MISMATCH";
    case Err::HARDWARE_CONFIG_DIRTY: return "HARDWARE_CONFIG_DIRTY";
    case Err::CLOCK_STALLED: return "CLOCK_STALLED";
    case Err::CANCELLED: return "CANCELLED";
    case Err::CONFIG_UNKNOWN: return "CONFIG_UNKNOWN";
    case Err::RESULT_NOT_AVAILABLE: return "RESULT_NOT_AVAILABLE";
    case Err::TOKEN_MISMATCH: return "TOKEN_MISMATCH";
    case Err::INDETERMINATE: return "INDETERMINATE";
    default: return "UNKNOWN";
  }
}

void printActiveAddress() {
  std::printf("  Active ADS1115 address: 0x%02X\n", activeI2cAddress);
  std::printf("  Requested ADS1115 address: 0x%02X\n", requestedI2cAddress);
  if (device.isInitialized()) {
    std::printf("  Initialized driver address: 0x%02X\n", activeI2cAddress);
  } else {
    std::printf("  Initialized driver address: NONE\n");
  }
  if (requestedI2cAddress != activeI2cAddress) {
    if (device.isInitialized()) {
      std::printf("  Address note: requested 0x%02X is not initialized; functional commands use 0x%02X\n",
                  requestedI2cAddress,
                  activeI2cAddress);
    } else {
      std::printf("  Address note: requested 0x%02X is not initialized; no functional address is ready\n",
                  requestedI2cAddress);
    }
  }
  if (!lastAddressSelectionStatus.ok()) {
    std::printf("  Last address selection error: %s detail=%ld msg=%s\n",
                errToStr(lastAddressSelectionStatus.code),
                static_cast<long>(lastAddressSelectionStatus.detail),
                lastAddressSelectionStatus.msg ? lastAddressSelectionStatus.msg : "");
  }
}

ADS1115::Config makeDriverConfig(uint8_t address) {
  ADS1115::Config cfg;
  cfg.i2cWrite = ads1115IdfWrite;
  cfg.i2cWriteRead = ads1115IdfWriteRead;
  cfg.i2cUser = &ads1115IdfTransportContext();
  cfg.nowMs = ads1115IdfNowMs;
  cfg.cooperativeYield = ads1115IdfYield;
  cfg.i2cAddress = address;
  cfg.i2cTimeoutMs = I2C_TIMEOUT_MS;
  cfg.offlineThreshold = 5;
  if (ALERT_RDY_PIN >= 0) {
    cfg.alertRdyPin = ALERT_RDY_PIN;
    cfg.gpioRead = readAlertRdyPin;
  }
  return cfg;
}

ADS1115::Status probeAddressRaw(uint8_t address) {
  Ads1115IdfI2cTransport& ctx = ads1115IdfTransportContext();
  if (ctx.bus == nullptr) {
    return ADS1115::Status::Error(ADS1115::Err::INVALID_CONFIG, "IDF I2C bus missing");
  }
  const esp_err_t err = i2c_master_probe(ctx.bus, address, I2C_TIMEOUT_MS);
  if (err == ESP_OK) {
    return ADS1115::Status::Ok();
  }
  if (err == ESP_ERR_TIMEOUT) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "ESP-IDF I2C probe timeout", err);
  }
  if (err == ESP_ERR_INVALID_ARG || err == ESP_ERR_INVALID_STATE) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_BUS, "ESP-IDF I2C probe bus error", err);
  }
  return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "ESP-IDF I2C probe error", err);
}

ADS1115::Status beginDriverAtAddress(uint8_t address) {
  if (!isValidAds1115Address(address)) {
    lastAddressSelectionStatus =
        ADS1115::Status::Error(ADS1115::Err::INVALID_PARAM,
                               "Invalid ADS1115 address",
                               static_cast<int32_t>(address));
    return lastAddressSelectionStatus;
  }

  requestedI2cAddress = address;
  ADS1115::Status st = probeAddressRaw(address);
  if (!st.ok()) {
    lastAddressSelectionStatus = st;
    return st;
  }

  device.end();
  if (!ads1115IdfInitI2c(I2C_SDA, I2C_SCL, I2C_FREQ_HZ, I2C_TIMEOUT_MS, address)) {
    lastAddressSelectionStatus =
        ADS1115::Status::Error(ADS1115::Err::I2C_BUS,
                               "ESP-IDF I2C address reinit failed",
                               ads1115IdfLastError());
    return lastAddressSelectionStatus;
  }

  st = device.begin(makeDriverConfig(address));
  lastAddressSelectionStatus = st;
  if (st.ok()) {
    activeI2cAddress = address;
    requestedI2cAddress = address;
  }
  return st;
}

const char* stateToStr(ADS1115::DriverState state) {
  using ADS1115::DriverState;
  switch (state) {
    case DriverState::UNINIT: return "UNINIT";
    case DriverState::READY: return "READY";
    case DriverState::DEGRADED: return "DEGRADED";
    case DriverState::OFFLINE: return "OFFLINE";
    default: return "UNKNOWN";
  }
}

const char* jobStateToStr(ADS1115::JobState state) {
  using ADS1115::JobState;
  switch (state) {
    case JobState::IDLE: return "IDLE";
    case JobState::SINGLE_SHOT_WRITE_CONFIG: return "SINGLE_SHOT_WRITE_CONFIG";
    case JobState::SINGLE_SHOT_WAIT_CONVERSION: return "SINGLE_SHOT_WAIT_CONVERSION";
    case JobState::SINGLE_SHOT_POLL_READY: return "SINGLE_SHOT_POLL_READY";
    case JobState::SINGLE_SHOT_READ_CONVERSION: return "SINGLE_SHOT_READ_CONVERSION";
    case JobState::APPLY_WRITE_LOW_THRESHOLD: return "APPLY_WRITE_LOW_THRESHOLD";
    case JobState::APPLY_WRITE_HIGH_THRESHOLD: return "APPLY_WRITE_HIGH_THRESHOLD";
    case JobState::APPLY_WRITE_CONFIG: return "APPLY_WRITE_CONFIG";
    case JobState::APPLY_VERIFY_LOW_THRESHOLD: return "APPLY_VERIFY_LOW_THRESHOLD";
    case JobState::APPLY_VERIFY_HIGH_THRESHOLD: return "APPLY_VERIFY_HIGH_THRESHOLD";
    case JobState::APPLY_VERIFY_CONFIG: return "APPLY_VERIFY_CONFIG";
    case JobState::COMPLETE: return "COMPLETE";
    case JobState::FAILED: return "FAILED";
    default: return "UNKNOWN";
  }
}

const char* muxToStr(ADS1115::Mux mux) {
  using ADS1115::Mux;
  switch (mux) {
    case Mux::AIN0_AIN1: return "AIN0_AIN1";
    case Mux::AIN0_AIN3: return "AIN0_AIN3";
    case Mux::AIN1_AIN3: return "AIN1_AIN3";
    case Mux::AIN2_AIN3: return "AIN2_AIN3";
    case Mux::AIN0_GND: return "AIN0_GND";
    case Mux::AIN1_GND: return "AIN1_GND";
    case Mux::AIN2_GND: return "AIN2_GND";
    case Mux::AIN3_GND: return "AIN3_GND";
    default: return "UNKNOWN";
  }
}

const char* gainToStr(ADS1115::Gain gain) {
  using ADS1115::Gain;
  switch (gain) {
    case Gain::FSR_6_144V: return "FSR_6_144V";
    case Gain::FSR_4_096V: return "FSR_4_096V";
    case Gain::FSR_2_048V: return "FSR_2_048V";
    case Gain::FSR_1_024V: return "FSR_1_024V";
    case Gain::FSR_0_512V: return "FSR_0_512V";
    case Gain::FSR_0_256V: return "FSR_0_256V";
    default: return "UNKNOWN";
  }
}

const char* rateToStr(ADS1115::DataRate rate) {
  using ADS1115::DataRate;
  switch (rate) {
    case DataRate::SPS_8: return "SPS_8";
    case DataRate::SPS_16: return "SPS_16";
    case DataRate::SPS_32: return "SPS_32";
    case DataRate::SPS_64: return "SPS_64";
    case DataRate::SPS_128: return "SPS_128";
    case DataRate::SPS_250: return "SPS_250";
    case DataRate::SPS_475: return "SPS_475";
    case DataRate::SPS_860: return "SPS_860";
    default: return "UNKNOWN";
  }
}

const char* modeToStr(ADS1115::Mode mode) {
  return (mode == ADS1115::Mode::CONTINUOUS) ? "CONTINUOUS" : "SINGLE_SHOT";
}

const char* compModeToStr(ADS1115::ComparatorMode mode) {
  return (mode == ADS1115::ComparatorMode::WINDOW) ? "WINDOW" : "TRADITIONAL";
}

const char* compPolToStr(ADS1115::ComparatorPolarity polarity) {
  return (polarity == ADS1115::ComparatorPolarity::ACTIVE_HIGH) ? "ACTIVE_HIGH" : "ACTIVE_LOW";
}

const char* compLatchToStr(ADS1115::ComparatorLatch latch) {
  return (latch == ADS1115::ComparatorLatch::LATCHING) ? "LATCHING" : "NON_LATCHING";
}

const char* compQueueToStr(ADS1115::ComparatorQueue queue) {
  using ADS1115::ComparatorQueue;
  switch (queue) {
    case ComparatorQueue::ASSERT_1: return "ASSERT_1";
    case ComparatorQueue::ASSERT_2: return "ASSERT_2";
    case ComparatorQueue::ASSERT_4: return "ASSERT_4";
    case ComparatorQueue::DISABLE: return "DISABLE";
    default: return "UNKNOWN";
  }
}

ADS1115::Mux channelToMux(int channel) {
  switch (channel) {
    case 1: return ADS1115::Mux::AIN1_GND;
    case 2: return ADS1115::Mux::AIN2_GND;
    case 3: return ADS1115::Mux::AIN3_GND;
    default: return ADS1115::Mux::AIN0_GND;
  }
}

ADS1115::Mux diffToMux(int index) {
  switch (index) {
    case 1: return ADS1115::Mux::AIN0_AIN3;
    case 2: return ADS1115::Mux::AIN1_AIN3;
    case 3: return ADS1115::Mux::AIN2_AIN3;
    default: return ADS1115::Mux::AIN0_AIN1;
  }
}

const char* resultColor(bool ok) {
  return ok ? COLOR_GREEN : COLOR_RED;
}

void printStatus(const ADS1115::Status& st) {
  std::printf("  Status: %s%s%s (code=%u, detail=%ld)\n",
              resultColor(st.ok()),
              errToStr(st.code),
              COLOR_RESET,
              static_cast<unsigned>(st.code),
              static_cast<long>(st.detail));
  if (st.msg != nullptr && st.msg[0] != '\0') {
    std::printf("  Message: %s%s%s\n", COLOR_YELLOW, st.msg, COLOR_RESET);
  }
}

ADS1115::Status applyCachedProfileVerified() {
  ADS1115::Status st = device.startApplyConfigJob();
  if (!st.inProgress()) {
    return st;
  }
  for (uint8_t step = 0; step < 3U; ++step) {
    const ADS1115::PollResult progress = device.pollApplyConfig(nowMs(), 3);
    if (!progress.done) {
      continue;
    }
    ADS1115::OperationResult terminal;
    st = device.takeResult(progress.token, terminal);
    return st.ok() ? terminal.status : st;
  }
  const ADS1115::OperationToken token = device.activeOperationToken();
  device.cancelJob();
  ADS1115::OperationResult terminal;
  if (device.terminalResultAvailable()) {
    (void)device.takeResult(token, terminal);
  }
  return ADS1115::Status::Error(ADS1115::Err::INDETERMINATE,
                                "Config verification did not terminate");
}

ADS1115::Status mutateAndVerify(const ADS1115::Status& mutation) {
  return mutation.ok() ? applyCachedProfileVerified() : mutation;
}

struct HealthSnapshot {
  ADS1115::DriverState state = ADS1115::DriverState::UNINIT;
  bool online = false;
  uint8_t consecutiveFailures = 0;
  uint32_t totalSuccess = 0;
  uint32_t totalFailures = 0;

  void capture() {
    state = device.state();
    online = device.isOnline();
    consecutiveFailures = device.consecutiveFailures();
    totalSuccess = device.totalSuccess();
    totalFailures = device.totalFailures();
  }
};

void printHealthDiff(const HealthSnapshot& before, const HealthSnapshot& after) {
  bool changed = false;
  if (before.state != after.state) {
    std::printf("  State: %s -> %s\n", stateToStr(before.state), stateToStr(after.state));
    changed = true;
  }
  if (before.online != after.online) {
    std::printf("  Online: %s -> %s\n", before.online ? "YES" : "NO",
                after.online ? "YES" : "NO");
    changed = true;
  }
  if (before.consecutiveFailures != after.consecutiveFailures) {
    std::printf("  ConsecFail: %u -> %u\n", before.consecutiveFailures,
                after.consecutiveFailures);
    changed = true;
  }
  if (before.totalSuccess != after.totalSuccess) {
    std::printf("  TotalOK: %lu -> %lu (+%lu)\n",
                static_cast<unsigned long>(before.totalSuccess),
                static_cast<unsigned long>(after.totalSuccess),
                static_cast<unsigned long>(after.totalSuccess - before.totalSuccess));
    changed = true;
  }
  if (before.totalFailures != after.totalFailures) {
    std::printf("  TotalFail: %lu -> %lu (+%lu)\n",
                static_cast<unsigned long>(before.totalFailures),
                static_cast<unsigned long>(after.totalFailures),
                static_cast<unsigned long>(after.totalFailures - before.totalFailures));
    changed = true;
  }
  if (!changed) {
    std::printf("  (no health changes)\n");
  }
}

void printDriverHealth() {
  const uint32_t now = nowMs();
  const uint32_t ok = device.totalSuccess();
  const uint32_t fail = device.totalFailures();
  const uint64_t total = static_cast<uint64_t>(ok) + static_cast<uint64_t>(fail);
  const float rate = total > 0U ? (100.0f * static_cast<float>(ok)) / static_cast<float>(total) : 0.0f;
  const ADS1115::Status lastErr = device.lastError();

  std::printf("=== Driver Health ===\n");
  std::printf("  State: %s\n", stateToStr(device.state()));
  std::printf("  Online: %s\n", device.isOnline() ? "YES" : "NO");
  std::printf("  Consecutive failures: %u\n", device.consecutiveFailures());
  std::printf("  Total success: %lu\n", static_cast<unsigned long>(ok));
  std::printf("  Total failures: %lu\n", static_cast<unsigned long>(fail));
  std::printf("  Success rate: %.1f%%\n", rate);
  if (device.lastOkMs() > 0U) {
    std::printf("  Last OK: %lu ms ago (at %lu ms)\n",
                static_cast<unsigned long>(now - device.lastOkMs()),
                static_cast<unsigned long>(device.lastOkMs()));
  } else {
    std::printf("  Last OK: never\n");
  }
  if (device.lastErrorMs() > 0U) {
    std::printf("  Last error: %lu ms ago (at %lu ms)\n",
                static_cast<unsigned long>(now - device.lastErrorMs()),
                static_cast<unsigned long>(device.lastErrorMs()));
  } else {
    std::printf("  Last error: never\n");
  }
  if (!lastErr.ok()) {
    std::printf("  Error code: %s\n", errToStr(lastErr.code));
    std::printf("  Error detail: %ld\n", static_cast<long>(lastErr.detail));
    if (lastErr.msg != nullptr && lastErr.msg[0] != '\0') {
      std::printf("  Error msg: %s\n", lastErr.msg);
    }
  }
}

void printCompactHealth() {
  const uint32_t ok = device.totalSuccess();
  const uint32_t fail = device.totalFailures();
  const uint64_t total = static_cast<uint64_t>(ok) + static_cast<uint64_t>(fail);
  const float rate = total > 0U ? (100.0f * static_cast<float>(ok)) / static_cast<float>(total) : 0.0f;
  std::printf("Health: state=%s online=%s consec=%u ok=%lu fail=%lu rate=%.1f%%\n",
              stateToStr(device.state()),
              device.isOnline() ? "YES" : "NO",
              device.consecutiveFailures(),
              static_cast<unsigned long>(ok),
              static_cast<unsigned long>(fail),
              rate);
}

void printHelpItem(const char* command, const char* desc) {
  std::printf("  %s%-32s%s - %s\n", COLOR_CYAN, command, COLOR_RESET, desc);
}

void printHelp() {
  std::printf("\n%s=== ADS1115 CLI Help ===%s\n", COLOR_CYAN, COLOR_RESET);
  std::printf("\n%s[Common]%s\n", COLOR_GREEN, COLOR_RESET);
  printHelpItem("help / ?", "Show this help");
  printHelpItem("version / ver", "Print firmware and library version info");
  printHelpItem("scan", "Scan I2C bus");
  printHelpItem("addr [0x48..0x4B]", "Show or select ADS1115 address");
  printHelpItem("read", "Read single conversion (blocking)");
  printHelpItem("readv", "Read single conversion as voltage (blocking)");
  printHelpItem("read N", "Read N conversions");
  printHelpItem("start", "Start single-shot conversion");
  printHelpItem("poll", "Check if conversion ready");
  printHelpItem("raw", "Read raw value");
  printHelpItem("voltage", "Read as voltage");
  printHelpItem("timing", "Print conversion time and LSB voltage");
  printHelpItem("job", "Show poll-chunked job state");
  printHelpItem("job single", "Start poll-chunked single-shot job");
  printHelpItem("job apply", "Start poll-chunked config apply job");
  printHelpItem("job poll [0..255]", "Poll active job with bounded instruction budget");
  printHelpItem("job cancel", "Cancel active poll-chunked job");

  std::printf("\n%s[Configuration]%s\n", COLOR_GREEN, COLOR_RESET);
  printHelpItem("ch [0|1|2|3]", "Set single-ended channel (AINx vs GND)");
  printHelpItem("diff [0|1|2|3]", "Set differential pair");
  printHelpItem("gain [0..5]", "Set PGA (0=6.144V, 2=2.048V, 5=0.256V)");
  printHelpItem("rate [0..7]", "Set data rate");
  printHelpItem("mode [single|cont]", "Set operating mode");
  printHelpItem("comp", "Show comparator config");
  printHelpItem("comp mode [trad|window]", "Set comparator mode");
  printHelpItem("comp pol [low|high]", "Set comparator polarity");
  printHelpItem("comp latch [0|1]", "Set comparator latch");
  printHelpItem("comp queue [1|2|4|disable]", "Set comparator queue");
  printHelpItem("comp th <low> <high>", "Set comparator thresholds");
  printHelpItem("comp rdy", "Enable conversion-ready pin mode");
  printHelpItem("comp disable", "Disable comparator");
  printHelpItem("config", "Dump config register");
  printHelpItem("config write <hex>", "Write full config register value");

  std::printf("\n%s[Registers]%s\n", COLOR_GREEN, COLOR_RESET);
  printHelpItem("reg <0..3>", "Read 16-bit ADS1115 register");
  printHelpItem("wreg <1..3> <val>", "Write writable register");

  std::printf("\n%s[Diagnostics]%s\n", COLOR_GREEN, COLOR_RESET);
  printHelpItem("drv", "Show driver state and health");
  printHelpItem("state", "Show compact one-line health summary");
  printHelpItem("probe", "Probe device (no health tracking)");
  printHelpItem("recover", "Manual recovery attempt");
  printHelpItem("shutdown", "Write single-shot idle while staying initialized");
  printHelpItem("cfg / settings", "Print active configuration snapshot");
  printHelpItem("verbose [0|1]", "Enable/disable verbose output");
  printHelpItem("stress [N]", "Run N conversion cycles");
  printHelpItem("stress_mix [N]", "Run N mixed-operation stress cycles");
  printHelpItem("selftest", "Run safe command self-test report");
}

void printVersionInfo() {
  std::printf("=== Version Info ===\n");
  std::printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  std::printf("  ADS1115 library version: %s\n", ADS1115::VERSION);
  std::printf("  ADS1115 library full: %s\n", ADS1115::VERSION_FULL);
  std::printf("  ADS1115 library build: %s\n", ADS1115::BUILD_TIMESTAMP);
  std::printf("  ADS1115 library commit: %s (%s)\n", ADS1115::GIT_COMMIT,
              ADS1115::GIT_STATUS);
  std::printf("  ADS1115 version code: %d (major=%d minor=%d patch=%d)\n",
              ADS1115::VERSION_INT,
              ADS1115::VERSION_MAJOR,
              ADS1115::VERSION_MINOR,
              ADS1115::VERSION_PATCH);
}

void scanBus() {
  Ads1115IdfI2cTransport& ctx = ads1115IdfTransportContext();
  if (ctx.bus == nullptr) {
    std::printf("I2C bus is not initialized\n");
    return;
  }
  std::printf("=== I2C Scan ===\n");
  uint32_t found = 0;
  for (uint8_t addr = 0x08U; addr <= 0x77U; ++addr) {
    const esp_err_t err = i2c_master_probe(ctx.bus, addr, I2C_TIMEOUT_MS);
    if (err == ESP_OK) {
      std::printf("  0x%02X%s\n", addr, addr == activeI2cAddress ? " (ADS1115 active)" : "");
      ++found;
    }
  }
  std::printf("Found %lu device(s)\n", static_cast<unsigned long>(found));
}

void printComparatorSettings() {
  int16_t low = 0;
  int16_t high = 0;
  ADS1115::Status st = device.getThresholds(low, high);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  std::printf("=== Comparator ===\n");
  std::printf("  Mode: %s\n", compModeToStr(device.getComparatorMode()));
  std::printf("  Polarity: %s\n", compPolToStr(device.getComparatorPolarity()));
  std::printf("  Latch: %s\n", compLatchToStr(device.getComparatorLatch()));
  std::printf("  Queue: %s\n", compQueueToStr(device.getComparatorQueue()));
  std::printf("  Threshold low/high: %d / %d\n", static_cast<int>(low), static_cast<int>(high));
  std::printf("  Conversion-ready mode: %s\n",
              device.isConversionReadyModeEnabled() ? "YES" : "NO");
  std::printf("  ALERT/RDY pin configured: %s\n",
              device.isAlertRdyPinConfigured() ? "YES" : "NO");
  std::printf("  Using ALERT/RDY pin: %s\n",
              device.usesAlertRdyPinForConversionReady() ? "YES" : "NO");
}

void printTimingInfo() {
  std::printf("=== Timing/Scale ===\n");
  std::printf("  Conversion time: %lu ms\n",
              static_cast<unsigned long>(device.getConversionTimeMs()));
  std::printf("  LSB voltage: %.9f V\n", device.getLsbVoltage());
}

void printCurrentMux() {
  const ADS1115::Mux mux = device.getMux();
  std::printf("  Mux: %s\n", muxToStr(mux));
}

void printConfig() {
  uint16_t config = 0;
  ADS1115::Status st = device.readConfig(config);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  std::printf("  Config: 0x%04X\n", config);
  std::printf("  Fields: OS=%u MUX=%s PGA=%s MODE=%s DR=%s\n",
              static_cast<unsigned>((config & ADS1115::cmd::MASK_OS) >> ADS1115::cmd::BIT_OS),
              muxToStr(static_cast<ADS1115::Mux>((config & ADS1115::cmd::MASK_MUX) >>
                                                 ADS1115::cmd::BIT_MUX)),
              gainToStr(static_cast<ADS1115::Gain>((config & ADS1115::cmd::MASK_PGA) >>
                                                   ADS1115::cmd::BIT_PGA)),
              (((config & ADS1115::cmd::MASK_MODE) >> ADS1115::cmd::BIT_MODE) ==
               static_cast<uint16_t>(ADS1115::Mode::CONTINUOUS)) ? "CONTINUOUS" : "SINGLE_SHOT",
              rateToStr(static_cast<ADS1115::DataRate>((config & ADS1115::cmd::MASK_DR) >>
                                                       ADS1115::cmd::BIT_DR)));
}

void printSettingsSnapshot() {
  ADS1115::SettingsSnapshot snap;
  ADS1115::Status st = device.getSettings(snap);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  std::printf("=== Cached Settings ===\n");
  std::printf("  Initialized: %s\n", snap.initialized ? "YES" : "NO");
  std::printf("  State: %s\n", stateToStr(snap.state));
  std::printf("  Address: 0x%02X\n", snap.i2cAddress);
  std::printf("  Timeout: %lu ms\n", static_cast<unsigned long>(snap.i2cTimeoutMs));
  std::printf("  Offline threshold: %u\n", static_cast<unsigned>(snap.offlineThreshold));
  std::printf("  Hooks: now=%s gpio=%s yield=%s\n",
              snap.hasNowMsHook ? "YES" : "NO",
              snap.hasGpioReadHook ? "YES" : "NO",
              snap.hasCooperativeYieldHook ? "YES" : "NO");
  std::printf("  Timebase available: %s\n",
              snap.timebaseAvailable ? "YES" : "NO");
  std::printf("  Hardware/cache dirty: %s\n",
              snap.hardwareConfigDirty ? "YES" : "NO");
  if (snap.hardwareConfigDirty) {
    std::printf("  Dirty error: %s detail=%ld msg=%s\n",
                errToStr(snap.hardwareConfigDirtyError.code),
                static_cast<long>(snap.hardwareConfigDirtyError.detail),
                snap.hardwareConfigDirtyError.msg ? snap.hardwareConfigDirtyError.msg : "");
    if (snap.hardwareConfigDirtyAddress != 0x00U) {
      std::printf("  Dirty address: 0x%02X\n", snap.hardwareConfigDirtyAddress);
    }
  }
  std::printf("  Alert pin: %d\n", snap.alertRdyPin);
  std::printf("  ALERT/RDY pin configured: %s\n", snap.alertRdyPinConfigured ? "YES" : "NO");
  std::printf("  Conversion-ready mode: %s\n", snap.conversionReadyModeEnabled ? "YES" : "NO");
  std::printf("  Using ALERT/RDY pin: %s\n", snap.usesAlertRdyPin ? "YES" : "NO");
  std::printf("  Mux: %s\n", muxToStr(snap.mux));
  std::printf("  Gain: %s\n", gainToStr(snap.gain));
  std::printf("  Rate: %s\n", rateToStr(snap.dataRate));
  std::printf("  Mode: %s\n", modeToStr(snap.mode));
  std::printf("  Comparator: mode=%s pol=%s latch=%s queue=%s\n",
              compModeToStr(snap.compMode),
              compPolToStr(snap.compPolarity),
              compLatchToStr(snap.compLatch),
              compQueueToStr(snap.compQueue));
  std::printf("  Thresholds: low=%d high=%d\n", static_cast<int>(snap.compThresholdLow),
              static_cast<int>(snap.compThresholdHigh));
  std::printf("  Conversion: started=%s ready=%s start=%lu ms lastRaw=%d\n",
              snap.conversionStarted ? "YES" : "NO",
              snap.conversionReady ? "YES" : "NO",
              static_cast<unsigned long>(snap.conversionStartMs),
              static_cast<int>(snap.lastRawValue));
}

bool isSingleShotJobState(ADS1115::JobState state) {
  using ADS1115::JobState;
  return state == JobState::SINGLE_SHOT_WRITE_CONFIG ||
         state == JobState::SINGLE_SHOT_WAIT_CONVERSION ||
         state == JobState::SINGLE_SHOT_POLL_READY ||
         state == JobState::SINGLE_SHOT_READ_CONVERSION ||
         state == JobState::WAIT_IDLE_AFTER_ABANDON;
}

bool isApplyJobState(ADS1115::JobState state) {
  using ADS1115::JobState;
  return state == JobState::APPLY_WRITE_LOW_THRESHOLD ||
         state == JobState::APPLY_WRITE_HIGH_THRESHOLD ||
         state == JobState::APPLY_WRITE_CONFIG ||
         state == JobState::APPLY_VERIFY_LOW_THRESHOLD ||
         state == JobState::APPLY_VERIFY_HIGH_THRESHOLD ||
         state == JobState::APPLY_VERIFY_CONFIG;
}

void printJobStatus() {
  std::printf("=== Job Status ===\n");
  std::printf("  Active: %s\n", device.jobActive() ? "YES" : "NO");
  std::printf("  State: %s\n", jobStateToStr(device.jobState()));
  std::printf("  Last raw: %d\n", static_cast<int>(device.lastRawValue()));
  std::printf("  Last status:\n");
  printStatus(device.lastJobStatus());
}

void printPollResult(const ADS1115::PollResult& result) {
  std::printf("=== Job Poll Result ===\n");
  printStatus(result.status);
  std::printf("  Instructions used: %u\n", static_cast<unsigned>(result.instructionsUsed));
  std::printf("  Done: %s\n", result.done ? "YES" : "NO");
  std::printf("  State: %s\n", jobStateToStr(result.state));
  std::printf("  Last raw: %d\n", static_cast<int>(device.lastRawValue()));
}

void printAndAcknowledgePollResult(const ADS1115::PollResult& result) {
  printPollResult(result);
  if (!result.done || !result.token.valid()) {
    return;
  }
  ADS1115::OperationResult terminal;
  const ADS1115::Status st = device.takeResult(result.token, terminal);
  if (!st.ok()) {
    std::printf("  Terminal result acknowledgement failed:\n");
    printStatus(st);
  }
}

ADS1115::Status startDiagnosticSingleShotJob() {
  ADS1115::ChannelRequest request;
  request.mux = device.getMux();
  request.gain = device.getGain();
  const uint32_t now = nowMs();
  ADS1115::OperationToken token;
  return device.startRead(request, now, now + DIAGNOSTIC_JOB_TIMEOUT_MS, token);
}

ADS1115::Status startDiagnosticApplyJob() {
  ADS1115::AppliedProfileSnapshot applied;
  ADS1115::Status st = device.getAppliedProfile(applied);
  if (!st.ok()) {
    return st;
  }
  const uint32_t now = nowMs();
  ADS1115::OperationToken token;
  return device.startApplyProfile(
      applied.profile, now, now + DIAGNOSTIC_JOB_TIMEOUT_MS, token);
}

void handleJobCommand(const char* cmd) {
  const char* arg = nullptr;
  if (std::strcmp(cmd, "job") == 0) {
    printJobStatus();
  } else if (std::strcmp(cmd, "job single") == 0) {
    printStatus(startDiagnosticSingleShotJob());
    printJobStatus();
  } else if (std::strcmp(cmd, "job apply") == 0) {
    printStatus(startDiagnosticApplyJob());
    printJobStatus();
  } else if (std::strcmp(cmd, "job cancel") == 0) {
    const ADS1115::OperationToken token = device.activeOperationToken();
    device.cancelJob();
    if (token.valid() && device.terminalResultAvailable()) {
      ADS1115::OperationResult terminal;
      (void)device.takeResult(token, terminal);
    }
    printJobStatus();
  } else if (std::strcmp(cmd, "job poll") == 0 ||
             (arg = argAfter(cmd, "job poll ")) != nullptr) {
    uint32_t budget = 1;
    if (arg != nullptr && (!parseU32(arg, budget) || budget > 255U)) {
      std::printf("Usage: job poll [0..255]\n");
      return;
    }
    const ADS1115::JobState state = device.jobState();
    if (isSingleShotJobState(state)) {
      printAndAcknowledgePollResult(
          device.pollSingleShot(nowMs(), static_cast<uint8_t>(budget)));
    } else if (isApplyJobState(state)) {
      printAndAcknowledgePollResult(
          device.pollApplyConfig(nowMs(), static_cast<uint8_t>(budget)));
    } else {
      std::printf("No active pollable job\n");
      printJobStatus();
    }
  } else {
    std::printf("Usage: job [single|apply|poll [0..255]|cancel]\n");
  }
}

uint32_t stressProgressStep(uint32_t total) {
  if (total == 0U) {
    return 0U;
  }
  const uint32_t step = total / STRESS_PROGRESS_UPDATES;
  return step == 0U ? 1U : step;
}

void printStressProgress(uint32_t completed, uint32_t total, uint32_t ok, uint32_t fail) {
  const uint32_t step = stressProgressStep(total);
  if (step == 0U || (completed != total && (completed % step) != 0U)) {
    return;
  }
  const float pct = (100.0f * static_cast<float>(completed)) / static_cast<float>(total);
  std::printf("  Progress: %lu/%lu (%.0f%%, ok=%lu, fail=%lu)\n",
              static_cast<unsigned long>(completed),
              static_cast<unsigned long>(total),
              pct,
              static_cast<unsigned long>(ok),
              static_cast<unsigned long>(fail));
}

void runStress(uint32_t count) {
  const uint32_t start = nowMs();
  const HealthSnapshot before = [] {
    HealthSnapshot s;
    s.capture();
    return s;
  }();
  uint32_t ok = 0;
  uint32_t fail = 0;
  ADS1115::Status firstFailure = ADS1115::Status::Ok();
  ADS1115::Status lastFailure = ADS1115::Status::Ok();
  for (uint32_t i = 0; i < count; ++i) {
    int16_t raw = 0;
    ADS1115::Status st = device.readBlocking(raw);
    if (st.ok()) {
      ++ok;
      if (verboseMode) {
        std::printf("  %lu: %d (%.6f V)\n", static_cast<unsigned long>(i + 1U), raw,
                    device.rawToVoltage(raw));
      }
    } else {
      ++fail;
      if (firstFailure.ok()) {
        firstFailure = st;
      }
      lastFailure = st;
    }
    printStressProgress(i + 1U, count, ok, fail);
  }
  const uint32_t elapsed = nowMs() - start;
  HealthSnapshot after;
  after.capture();
  std::printf("=== Stress Summary ===\n");
  std::printf("  Total: %lu\n", static_cast<unsigned long>(count));
  std::printf("  Success: %lu\n", static_cast<unsigned long>(ok));
  std::printf("  Errors: %lu\n", static_cast<unsigned long>(fail));
  std::printf("  Success rate: %.2f%%\n",
              count > 0U ? (100.0f * static_cast<float>(ok)) / static_cast<float>(count) : 0.0f);
  std::printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  if (elapsed > 0U) {
    std::printf("  Rate: %.2f samples/s\n", (1000.0f * static_cast<float>(count)) /
                                                 static_cast<float>(elapsed));
  }
  std::printf("  Health changes:\n");
  printHealthDiff(before, after);
  if (!firstFailure.ok()) {
    std::printf("  First failure:\n");
    printStatus(firstFailure);
    std::printf("  Last failure:\n");
    printStatus(lastFailure);
  }
}

bool restoreStressBaseline(const ADS1115::SettingsSnapshot& baseline, ADS1115::Status& failure) {
  failure = device.setMode(baseline.mode);
  if (!failure.ok()) return false;
  failure = device.setMux(baseline.mux);
  if (!failure.ok()) return false;
  failure = device.setGain(baseline.gain);
  if (!failure.ok()) return false;
  failure = device.setDataRate(baseline.dataRate);
  if (!failure.ok()) return false;
  failure = applyCachedProfileVerified();
  return failure.ok();
}

void runStressMix(uint32_t count) {
  ADS1115::SettingsSnapshot baseline;
  const bool haveBaseline = device.getSettings(baseline).ok() && baseline.initialized;
  const uint32_t start = nowMs();
  HealthSnapshot before;
  before.capture();
  uint32_t ok = 0;
  uint32_t fail = 0;
  ADS1115::Status firstFailure = ADS1115::Status::Ok();
  ADS1115::Status lastFailure = ADS1115::Status::Ok();

  for (uint32_t i = 0; i < count; ++i) {
    ADS1115::Status st = ADS1115::Status::Ok();
    switch (i % 8U) {
      case 0: st = mutateAndVerify(device.setMux(channelToMux(static_cast<int>(i % 4U)))); break;
      case 1: st = mutateAndVerify(device.setGain(static_cast<ADS1115::Gain>(i % 6U))); break;
      case 2: st = mutateAndVerify(device.setDataRate(static_cast<ADS1115::DataRate>(i % 8U))); break;
      case 3: st = device.startConversion(); break;
      case 4: {
        bool ready = false;
        st = device.readConversionReady(ready);
        break;
      }
      case 5: {
        int16_t raw = 0;
        st = device.readRaw(raw);
        break;
      }
      case 6: {
        uint16_t cfg = 0;
        st = device.readConfig(cfg);
        break;
      }
      default: {
        float volts = 0.0f;
        st = device.readVoltage(volts);
        break;
      }
    }
    const bool operationOk =
        st.ok() || st.inProgress() || st.code == ADS1115::Err::CONVERSION_NOT_READY;
    ADS1115::Status serviceStatus = device.service(nowMs());
    const bool serviceOk = serviceStatus.ok();
    if (!serviceOk && verboseMode) {
      std::printf("service() reported an I2C/status issue during stress_mix\n");
      printStatus(serviceStatus);
    }
    if (operationOk && serviceOk) {
      ++ok;
    } else {
      ++fail;
      const ADS1115::Status failure = operationOk ? serviceStatus : st;
      if (firstFailure.ok()) {
        firstFailure = failure;
      }
      lastFailure = failure;
    }
    printStressProgress(i + 1U, count, ok, fail);
  }

  ADS1115::Status restoreFailure = ADS1115::Status::Ok();
  const bool restored = haveBaseline && restoreStressBaseline(baseline, restoreFailure);
  const uint32_t elapsed = nowMs() - start;
  HealthSnapshot after;
  after.capture();

  std::printf("=== stress_mix summary ===\n");
  std::printf("  Total: ok=%lu fail=%lu (%.2f%%)\n",
              static_cast<unsigned long>(ok),
              static_cast<unsigned long>(fail),
              count > 0U ? (100.0f * static_cast<float>(ok)) / static_cast<float>(count) : 0.0f);
  std::printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  if (elapsed > 0U) {
    std::printf("  Rate: %.2f ops/s\n", (1000.0f * static_cast<float>(count)) /
                                           static_cast<float>(elapsed));
  }
  std::printf("  Baseline restore: %s\n", restored ? "OK" : "FAILED/SKIPPED");
  if (!restored && !restoreFailure.ok()) {
    printStatus(restoreFailure);
  }
  std::printf("  Health changes:\n");
  printHealthDiff(before, after);
  if (!firstFailure.ok()) {
    std::printf("  First failure:\n");
    printStatus(firstFailure);
    std::printf("  Last failure:\n");
    printStatus(lastFailure);
  }
}

struct SelftestStats {
  uint32_t pass = 0;
  uint32_t fail = 0;
  uint32_t skip = 0;
};

void reportSelftest(SelftestStats& stats, const char* name, bool passed, const char* note = "") {
  if (passed) {
    ++stats.pass;
  } else {
    ++stats.fail;
  }
  std::printf("  [%s] %s", passed ? "PASS" : "FAIL", name);
  if (note != nullptr && note[0] != '\0') {
    std::printf(" - %s", note);
  }
  std::printf("\n");
}

void skipSelftest(SelftestStats& stats, const char* name, const char* note) {
  ++stats.skip;
  std::printf("  [SKIP] %s - %s\n", name, note);
}

void runSelfTest() {
  SelftestStats stats;
  std::printf("=== ADS1115 selftest (safe commands) ===\n");
  const uint32_t succBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint8_t consBefore = device.consecutiveFailures();

  ADS1115::Status st = device.probe();
  if (st.code == ADS1115::Err::NOT_INITIALIZED) {
    skipSelftest(stats, "probe responds", "driver not initialized");
    skipSelftest(stats, "remaining checks", "selftest aborted");
    std::printf("Selftest result: pass=%lu fail=%lu skip=%lu\n",
                static_cast<unsigned long>(stats.pass),
                static_cast<unsigned long>(stats.fail),
                static_cast<unsigned long>(stats.skip));
    return;
  }
  reportSelftest(stats, "probe responds", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportSelftest(stats, "probe no-health-side-effects",
                 device.totalSuccess() == succBefore &&
                     device.totalFailures() == failBefore &&
                     device.consecutiveFailures() == consBefore);

  uint16_t cfg = 0;
  st = device.readConfig(cfg);
  reportSelftest(stats, "readConfig", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setMode(ADS1115::Mode::SINGLE_SHOT);
  reportSelftest(stats, "setMode(single)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setMode(ADS1115::Mode::CONTINUOUS);
  reportSelftest(stats, "setMode(continuous)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setMode(ADS1115::Mode::SINGLE_SHOT);
  reportSelftest(stats, "restore mode(single)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setGain(ADS1115::Gain::FSR_2_048V);
  reportSelftest(stats, "setGain(2.048V)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setDataRate(ADS1115::DataRate::SPS_128);
  reportSelftest(stats, "setRate(128sps)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setMux(ADS1115::Mux::AIN0_GND);
  reportSelftest(stats, "setMux(AIN0_GND)", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = applyCachedProfileVerified();
  reportSelftest(stats, "apply+verify typed-read profile", st.ok(),
                 st.ok() ? "" : errToStr(st.code));
  st = device.startConversion();
  const bool started = st.ok() || st.inProgress();
  reportSelftest(stats, "startConversion", started, started ? "" : errToStr(st.code));
  if (started) {
    const uint32_t waitStart = nowMs();
    bool ready = false;
    ADS1115::Status pollStatus = ADS1115::Status::Ok();
    while ((nowMs() - waitStart) < 200U) {
      pollStatus = device.readConversionReady(ready);
      if (!pollStatus.ok() || ready) {
        break;
      }
      pollStatus = device.service(nowMs());
      if (!pollStatus.ok()) {
        break;
      }
      sleepMs(1U);
    }
    reportSelftest(stats, "poll after start", pollStatus.ok() && ready,
                   pollStatus.ok() ? "" : errToStr(pollStatus.code));
    int16_t raw = 0;
    pollStatus = device.readRaw(raw);
    reportSelftest(stats, "readRaw(after start)", pollStatus.ok(),
                   pollStatus.ok() ? "" : errToStr(pollStatus.code));
  }
  float volts = 0.0f;
  st = device.readVoltage(volts);
  reportSelftest(stats, "readVoltage", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.readBlockingVoltage(volts);
  reportSelftest(stats, "readBlockingVoltage", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorMode(ADS1115::ComparatorMode::TRADITIONAL);
  reportSelftest(stats, "setComparatorMode", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorPolarity(ADS1115::ComparatorPolarity::ACTIVE_LOW);
  reportSelftest(stats, "setComparatorPolarity", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorLatch(ADS1115::ComparatorLatch::NON_LATCHING);
  reportSelftest(stats, "setComparatorLatch", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorQueue(ADS1115::ComparatorQueue::DISABLE);
  reportSelftest(stats, "setComparatorQueue", st.ok(), st.ok() ? "" : errToStr(st.code));
  int16_t low = 0;
  int16_t high = 0;
  st = device.getThresholds(low, high);
  reportSelftest(stats, "getThresholds", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.recover();
  reportSelftest(stats, "recover", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportSelftest(stats, "isOnline", device.isOnline());
  std::printf("Selftest result: pass=%lu fail=%lu skip=%lu\n",
              static_cast<unsigned long>(stats.pass),
              static_cast<unsigned long>(stats.fail),
              static_cast<unsigned long>(stats.skip));
}

void handleCompCommand(const char* cmd) {
  const char* arg = nullptr;
  if (std::strcmp(cmd, "comp") == 0) {
    printComparatorSettings();
  } else if ((arg = argAfter(cmd, "comp mode ")) != nullptr) {
    if (std::strcmp(arg, "trad") == 0 || std::strcmp(arg, "traditional") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorMode(ADS1115::ComparatorMode::TRADITIONAL)));
    } else if (std::strcmp(arg, "window") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorMode(ADS1115::ComparatorMode::WINDOW)));
    } else {
      std::printf("Usage: comp mode [trad|window]\n");
    }
  } else if ((arg = argAfter(cmd, "comp pol ")) != nullptr) {
    if (std::strcmp(arg, "low") == 0 || std::strcmp(arg, "active_low") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorPolarity(ADS1115::ComparatorPolarity::ACTIVE_LOW)));
    } else if (std::strcmp(arg, "high") == 0 || std::strcmp(arg, "active_high") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorPolarity(ADS1115::ComparatorPolarity::ACTIVE_HIGH)));
    } else {
      std::printf("Usage: comp pol [low|high]\n");
    }
  } else if ((arg = argAfter(cmd, "comp latch ")) != nullptr) {
    int32_t value = 0;
    if (!parseI32(arg, value) || (value != 0 && value != 1)) {
      std::printf("Usage: comp latch [0|1]\n");
      return;
    }
    printStatus(mutateAndVerify(
        device.setComparatorLatch(value == 0 ? ADS1115::ComparatorLatch::NON_LATCHING
                                              : ADS1115::ComparatorLatch::LATCHING)));
  } else if ((arg = argAfter(cmd, "comp queue ")) != nullptr) {
    if (std::strcmp(arg, "1") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorQueue(ADS1115::ComparatorQueue::ASSERT_1)));
    } else if (std::strcmp(arg, "2") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorQueue(ADS1115::ComparatorQueue::ASSERT_2)));
    } else if (std::strcmp(arg, "4") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorQueue(ADS1115::ComparatorQueue::ASSERT_4)));
    } else if (std::strcmp(arg, "disable") == 0 || std::strcmp(arg, "off") == 0) {
      printStatus(mutateAndVerify(
          device.setComparatorQueue(ADS1115::ComparatorQueue::DISABLE)));
    } else {
      std::printf("Usage: comp queue [1|2|4|disable]\n");
    }
  } else if ((arg = argAfter(cmd, "comp th ")) != nullptr) {
    char args[MAX_LINE_LEN] = {};
    std::strncpy(args, arg, sizeof(args) - 1U);
    char* split = std::strchr(args, ' ');
    if (split == nullptr) {
      std::printf("Usage: comp th <low> <high>\n");
      return;
    }
    *split = '\0';
    char* highToken = split + 1;
    trimInPlace(args);
    trimInPlace(highToken);
    int32_t low = 0;
    int32_t high = 0;
    if (!parseI32(args, low) || !parseI32(highToken, high) ||
        low < -32768 || low > 32767 || high < -32768 || high > 32767) {
      std::printf("Thresholds must be in int16 range\n");
      return;
    }
    printStatus(mutateAndVerify(
        device.setThresholds(static_cast<int16_t>(low), static_cast<int16_t>(high))));
  } else if (std::strcmp(cmd, "comp rdy") == 0) {
    printStatus(device.enableConversionReadyPin());
  } else if (std::strcmp(cmd, "comp disable") == 0) {
    printStatus(mutateAndVerify(device.disableComparator()));
  } else {
    std::printf("Usage: comp [mode|pol|latch|queue|th|rdy|disable]\n");
  }
}

void processCommand(char* cmd) {
  trimInPlace(cmd);
  if (cmd[0] == '\0') {
    return;
  }

  const char* arg = nullptr;
  if (std::strcmp(cmd, "help") == 0 || std::strcmp(cmd, "?") == 0) {
    printHelp();
  } else if (std::strcmp(cmd, "version") == 0 || std::strcmp(cmd, "ver") == 0) {
    printVersionInfo();
  } else if (std::strcmp(cmd, "scan") == 0) {
    scanBus();
  } else if (std::strcmp(cmd, "state") == 0) {
    printActiveAddress();
    printCompactHealth();
  } else if (std::strcmp(cmd, "addr") == 0) {
    printActiveAddress();
  } else if ((arg = argAfter(cmd, "addr ")) != nullptr) {
    uint32_t address = 0;
    if (!parseU32(arg, address) || !isValidAds1115Address(address)) {
      std::printf("Usage: addr [0x48|0x49|0x4A|0x4B]\n");
      return;
    }
    std::printf("Selecting ADS1115 address 0x%02lX\n",
                static_cast<unsigned long>(address));
    ADS1115::Status st = beginDriverAtAddress(static_cast<uint8_t>(address));
    printStatus(st);
    printActiveAddress();
    if (st.ok()) {
      printDriverHealth();
    } else {
      std::printf("Address selection failed; initialized driver was left unchanged\n");
    }
  } else if (std::strcmp(cmd, "drv") == 0) {
    printActiveAddress();
    printDriverHealth();
  } else if (std::strcmp(cmd, "probe") == 0) {
    printActiveAddress();
    std::printf("Probing device (no health tracking)...\n");
    HealthSnapshot before;
    before.capture();
    ADS1115::Status st =
        (!device.isInitialized() || requestedI2cAddress != activeI2cAddress)
            ? probeAddressRaw(requestedI2cAddress)
            : device.probe();
    printStatus(st);
    HealthSnapshot after;
    after.capture();
    std::printf("  Health changes:\n");
    printHealthDiff(before, after);
  } else if (std::strcmp(cmd, "recover") == 0) {
    std::printf("Attempting recovery...\n");
    HealthSnapshot before;
    before.capture();
    ADS1115::Status st = device.recover();
    printStatus(st);
    HealthSnapshot after;
    after.capture();
    std::printf("  Health changes:\n");
    printHealthDiff(before, after);
    printDriverHealth();
  } else if (std::strcmp(cmd, "shutdown") == 0) {
    ADS1115::Status st = device.shutdown();
    printStatus(st);
    if (st.ok()) {
      std::printf("  Mode: %s\n", modeToStr(device.getMode()));
    }
  } else if (std::strcmp(cmd, "verbose") == 0) {
    std::printf("Verbose mode: %s\n", verboseMode ? "ON" : "OFF");
  } else if ((arg = argAfter(cmd, "verbose ")) != nullptr) {
    bool value = false;
    if (!parseBool01(arg, value)) {
      std::printf("Usage: verbose [0|1]\n");
      return;
    }
    verboseMode = value;
    std::printf("Verbose mode: %s\n", verboseMode ? "ON" : "OFF");
  } else if (std::strcmp(cmd, "start") == 0) {
    printStatus(device.startConversion());
  } else if (std::strcmp(cmd, "poll") == 0) {
    bool ready = false;
    ADS1115::Status st = device.readConversionReady(ready);
    if (st.ok()) {
      std::printf("  Conversion ready: %s\n", ready ? "YES" : "NO");
    } else {
      printStatus(st);
    }
  } else if (std::strcmp(cmd, "raw") == 0) {
    int16_t raw = 0;
    ADS1115::Status st = device.readRaw(raw);
    if (st.ok()) {
      std::printf("  Raw: %d\n", raw);
      if (verboseMode) {
        std::printf("  Voltage: %.6f V\n", device.rawToVoltage(raw));
      }
    } else {
      printStatus(st);
    }
  } else if (std::strcmp(cmd, "voltage") == 0) {
    float volts = 0.0f;
    ADS1115::Status st = device.readVoltage(volts);
    if (st.ok()) {
      std::printf("  Voltage: %.6f V\n", volts);
    } else {
      printStatus(st);
    }
  } else if (std::strcmp(cmd, "readv") == 0) {
    float volts = 0.0f;
    ADS1115::Status st = device.readBlockingVoltage(volts);
    if (st.ok()) {
      std::printf("  Blocking voltage: %.6f V\n", volts);
    } else {
      printStatus(st);
    }
  } else if (std::strcmp(cmd, "read") == 0) {
    int16_t raw = 0;
    ADS1115::Status st = device.readBlocking(raw);
    if (st.ok()) {
      std::printf("  Raw: %d\n", raw);
      std::printf("  Voltage: %.6f V\n", device.rawToVoltage(raw));
    } else {
      printStatus(st);
    }
  } else if ((arg = argAfter(cmd, "read ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 10000) {
      std::printf("Invalid count (1-10000)\n");
      return;
    }
    for (int32_t i = 0; i < count; ++i) {
      int16_t raw = 0;
      ADS1115::Status st = device.readBlocking(raw);
      if (!st.ok()) {
        printStatus(st);
        break;
      }
      std::printf("  %ld: %d (%.6f V)\n", static_cast<long>(i + 1), raw,
                  device.rawToVoltage(raw));
    }
  } else if (std::strcmp(cmd, "ch") == 0) {
    printCurrentMux();
  } else if ((arg = argAfter(cmd, "ch ")) != nullptr) {
    int32_t channel = 0;
    if (!parseI32(arg, channel) || channel < 0 || channel > 3) {
      std::printf("Invalid channel\n");
      return;
    }
    printStatus(mutateAndVerify(
        device.setMux(channelToMux(static_cast<int>(channel)))));
  } else if (std::strcmp(cmd, "diff") == 0) {
    printCurrentMux();
  } else if ((arg = argAfter(cmd, "diff ")) != nullptr) {
    int32_t diff = 0;
    if (!parseI32(arg, diff) || diff < 0 || diff > 3) {
      std::printf("Invalid differential index\n");
      return;
    }
    printStatus(mutateAndVerify(device.setMux(diffToMux(static_cast<int>(diff)))));
  } else if (std::strcmp(cmd, "gain") == 0) {
    std::printf("  Gain: %u (%s)\n", static_cast<unsigned>(device.getGain()),
                gainToStr(device.getGain()));
  } else if ((arg = argAfter(cmd, "gain ")) != nullptr) {
    int32_t gain = 0;
    if (!parseI32(arg, gain) || gain < 0 || gain > 5) {
      std::printf("Invalid gain\n");
      return;
    }
    printStatus(mutateAndVerify(
        device.setGain(static_cast<ADS1115::Gain>(gain))));
  } else if (std::strcmp(cmd, "rate") == 0) {
    std::printf("  Rate: %u (%s)\n", static_cast<unsigned>(device.getDataRate()),
                rateToStr(device.getDataRate()));
  } else if ((arg = argAfter(cmd, "rate ")) != nullptr) {
    int32_t rate = 0;
    if (!parseI32(arg, rate) || rate < 0 || rate > 7) {
      std::printf("Invalid rate\n");
      return;
    }
    printStatus(mutateAndVerify(
        device.setDataRate(static_cast<ADS1115::DataRate>(rate))));
  } else if (std::strcmp(cmd, "mode") == 0) {
    std::printf("  Mode: %s\n", modeToStr(device.getMode()));
  } else if ((arg = argAfter(cmd, "mode ")) != nullptr) {
    if (std::strcmp(arg, "single") == 0) {
      printStatus(mutateAndVerify(device.setMode(ADS1115::Mode::SINGLE_SHOT)));
    } else if (std::strcmp(arg, "cont") == 0 || std::strcmp(arg, "continuous") == 0) {
      printStatus(mutateAndVerify(device.setMode(ADS1115::Mode::CONTINUOUS)));
    } else {
      std::printf("Invalid mode\n");
    }
  } else if (std::strcmp(cmd, "timing") == 0) {
    printTimingInfo();
  } else if (std::strcmp(cmd, "job") == 0 || startsWith(cmd, "job ")) {
    handleJobCommand(cmd);
  } else if (startsWith(cmd, "comp")) {
    handleCompCommand(cmd);
  } else if (std::strcmp(cmd, "config") == 0 || std::strcmp(cmd, "cfg") == 0 ||
             std::strcmp(cmd, "settings") == 0) {
    printConfig();
    printSettingsSnapshot();
  } else if ((arg = argAfter(cmd, "config write ")) != nullptr) {
    uint32_t value = 0;
    if (!parseU32(arg, value) || value > 0xFFFFU) {
      std::printf("Usage: config write <0..0xFFFF>\n");
      return;
    }
    ADS1115::Status st = device.writeConfig(static_cast<uint16_t>(value));
    printStatus(st);
    if (st.ok()) {
      printConfig();
    }
  } else if ((arg = argAfter(cmd, "reg ")) != nullptr) {
    uint32_t addr = 0;
    if (!parseU32(arg, addr) || addr > ADS1115::cmd::REG_HI_THRESH) {
      std::printf("Usage: reg <0..3>\n");
      return;
    }
    uint16_t value = 0;
    ADS1115::Status st = device.readRegister16(static_cast<uint8_t>(addr), value);
    if (st.ok()) {
      std::printf("  Reg 0x%02lX = 0x%04X (%u)\n", static_cast<unsigned long>(addr),
                  value, value);
    } else {
      printStatus(st);
    }
  } else if ((arg = argAfter(cmd, "wreg ")) != nullptr) {
    char args[MAX_LINE_LEN] = {};
    std::strncpy(args, arg, sizeof(args) - 1U);
    char* split = std::strchr(args, ' ');
    if (split == nullptr) {
      std::printf("Usage: wreg <1..3> <val>\n");
      return;
    }
    *split = '\0';
    uint32_t addr = 0;
    uint32_t value = 0;
    if (!parseU32(args, addr) || !parseU32(split + 1, value) ||
        addr < ADS1115::cmd::REG_CONFIG || addr > ADS1115::cmd::REG_HI_THRESH ||
        value > 0xFFFFU) {
      std::printf("Usage: wreg <1..3> <val>\n");
      return;
    }
    printStatus(device.writeRegister16(static_cast<uint8_t>(addr), static_cast<uint16_t>(value)));
  } else if (std::strcmp(cmd, "selftest") == 0) {
    runSelfTest();
  } else if (std::strcmp(cmd, "stress_mix") == 0) {
    runStressMix(50U);
  } else if ((arg = argAfter(cmd, "stress_mix ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 100000) {
      std::printf("Invalid count (1-100000)\n");
      return;
    }
    runStressMix(static_cast<uint32_t>(count));
  } else if (std::strcmp(cmd, "stress") == 0) {
    runStress(10U);
  } else if ((arg = argAfter(cmd, "stress ")) != nullptr) {
    int32_t count = 0;
    if (!parseI32(arg, count) || count <= 0 || count > 100000) {
      std::printf("Invalid count (1-100000)\n");
      return;
    }
    runStress(static_cast<uint32_t>(count));
  } else {
    std::printf("Unknown command: %s\n", cmd);
  }
}

bool initDriver() {
  std::printf("=== ADS1115 ESP-IDF Bringup Example ===\n");
  if (!ads1115IdfInitI2c(I2C_SDA, I2C_SCL, I2C_FREQ_HZ, I2C_TIMEOUT_MS,
                         DEFAULT_ADS1115_ADDRESS)) {
    std::printf("Failed to initialize I2C: %s\n",
                esp_err_to_name(ads1115IdfLastError()));
    return false;
  }
  activeI2cAddress = DEFAULT_ADS1115_ADDRESS;
  requestedI2cAddress = DEFAULT_ADS1115_ADDRESS;
  lastAddressSelectionStatus = ADS1115::Status::Ok();
  std::printf("I2C initialized (SDA=%d, SCL=%d)\n", I2C_SDA, I2C_SCL);
  initAlertRdyPin();
  scanBus();

  ADS1115::Status st = device.begin(makeDriverConfig(activeI2cAddress));
  if (!st.ok()) {
    lastAddressSelectionStatus = st;
    std::printf("Failed to initialize device\n");
    printStatus(st);
    return false;
  }
  std::printf("Device initialized successfully\n");
  printDriverHealth();
  return true;
}

}  // namespace

extern "C" void app_main(void) {
  (void)initDriver();
  std::printf("\nType 'help' for commands\n> ");
  std::fflush(stdout);

  char line[MAX_LINE_LEN] = {};
  while (true) {
    // Explicit staged jobs are owned by the `job poll` command so its callback
    // budget remains observable. Background service is only for direct
    // conversion/readiness paths.
    if (device.isInitialized() && !device.jobActive()) {
      ADS1115::Status serviceStatus = device.service(nowMs());
      if (!serviceStatus.ok() && verboseMode) {
        std::printf("service() reported an I2C/status issue\n");
        printStatus(serviceStatus);
      }
    }
    if (std::fgets(line, sizeof(line), stdin) != nullptr) {
      line[sizeof(line) - 1U] = '\0';
      processCommand(line);
      std::printf("> ");
      std::fflush(stdout);
    } else {
      sleepMs(10U);
    }
  }
}
