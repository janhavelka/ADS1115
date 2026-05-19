/// @file main.cpp
/// @brief ADS1115 basic bringup example
/// @note This is an EXAMPLE, not part of the library

#include <cstdlib>

#include <Arduino.h>

#include "examples/common/BoardConfig.h"
#include "examples/common/BusDiag.h"
#include "examples/common/CliStyle.h"
#include "examples/common/HealthView.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

#include "ADS1115/ADS1115.h"

// ============================================================================
// Globals
// ============================================================================

ADS1115::ADS1115 device;
bool verboseMode = false;
static constexpr uint32_t STRESS_PROGRESS_UPDATES = 10U;

// ============================================================================
// Helper Functions
// ============================================================================

const char* errToStr(ADS1115::Err err) {
  using ADS1115::Err;
  switch (err) {
    case Err::OK:                   return "OK";
    case Err::NOT_INITIALIZED:      return "NOT_INITIALIZED";
    case Err::INVALID_CONFIG:       return "INVALID_CONFIG";
    case Err::I2C_ERROR:            return "I2C_ERROR";
    case Err::TIMEOUT:              return "TIMEOUT";
    case Err::INVALID_PARAM:        return "INVALID_PARAM";
    case Err::DEVICE_NOT_FOUND:     return "DEVICE_NOT_FOUND";
    case Err::CONVERSION_NOT_READY: return "CONVERSION_NOT_READY";
    case Err::BUSY:                 return "BUSY";
    case Err::IN_PROGRESS:          return "IN_PROGRESS";
    case Err::I2C_NACK_ADDR:        return "I2C_NACK_ADDR";
    case Err::I2C_NACK_DATA:        return "I2C_NACK_DATA";
    case Err::I2C_TIMEOUT:          return "I2C_TIMEOUT";
    case Err::I2C_BUS:              return "I2C_BUS";
    default:                        return "UNKNOWN";
  }
}

const char* stateToStr(ADS1115::DriverState st) {
  using ADS1115::DriverState;
  switch (st) {
    case DriverState::UNINIT:   return "UNINIT";
    case DriverState::READY:    return "READY";
    case DriverState::DEGRADED: return "DEGRADED";
    case DriverState::OFFLINE:  return "OFFLINE";
    default:                    return "UNKNOWN";
  }
}

const char* stateColor(ADS1115::DriverState st, bool online, uint8_t consecutiveFailures) {
  if (st == ADS1115::DriverState::UNINIT) {
    return LOG_COLOR_YELLOW;
  }
  return LOG_COLOR_STATE(online, consecutiveFailures);
}

const char* goodIfZeroColor(uint32_t value) {
  return cli::zeroGoodColor(value);
}

const char* goodIfNonZeroColor(uint32_t value) {
  return cli::nonZeroGoodColor(value);
}

const char* onOffColor(bool enabled) {
  return cli::enabledColor(enabled);
}

const char* yesNoColor(bool value) {
  return cli::yesNoColor(value);
}

const char* skipCountColor(uint32_t value) {
  return cli::warningIfNonZeroColor(value);
}

const char* successRateColor(float pct) {
  return cli::successRateColor(pct);
}

uint32_t stressProgressStep(uint32_t total) {
  if (total == 0U) {
    return 0U;
  }
  const uint32_t step = total / STRESS_PROGRESS_UPDATES;
  return (step == 0U) ? 1U : step;
}

void printStressProgress(uint32_t completed, uint32_t total, uint32_t okCount, uint32_t failCount) {
  if (completed == 0U || total == 0U) {
    return;
  }
  const uint32_t step = stressProgressStep(total);
  if (step == 0U || (completed != total && (completed % step) != 0U)) {
    return;
  }
  const float pct = (100.0f * static_cast<float>(completed)) / static_cast<float>(total);
  Serial.printf("  Progress: %lu/%lu (%s%.0f%%%s, ok=%s%lu%s, fail=%s%lu%s)\n",
                static_cast<unsigned long>(completed),
                static_cast<unsigned long>(total),
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET,
                goodIfNonZeroColor(okCount),
                static_cast<unsigned long>(okCount),
                LOG_COLOR_RESET,
                goodIfZeroColor(failCount),
                static_cast<unsigned long>(failCount),
                LOG_COLOR_RESET);
}

const char* staleTimeColor(bool isErrorTimestamp) {
  // "never" for last error means no failures yet (good), while
  // "never" for last OK usually means no successful operation so far.
  return isErrorTimestamp ? LOG_COLOR_GREEN : LOG_COLOR_YELLOW;
}

void printStatus(const ADS1115::Status& st) {
  Serial.printf("  Status: %s%s%s (code=%u, detail=%ld)\n",
                LOG_COLOR_RESULT(st.ok()),
                errToStr(st.code),
                LOG_COLOR_RESET,
                static_cast<unsigned>(st.code),
                static_cast<long>(st.detail));
  if (st.msg && st.msg[0]) {
    Serial.printf("  Message: %s%s%s\n", LOG_COLOR_YELLOW, st.msg, LOG_COLOR_RESET);
  }
}

void printDriverHealth() {
  const uint32_t now = millis();
  const uint32_t totalOk = device.totalSuccess();
  const uint32_t totalFail = device.totalFailures();
  const uint32_t total = totalOk + totalFail;
  const float successRate = (total > 0U)
                                ? (100.0f * static_cast<float>(totalOk) / static_cast<float>(total))
                                : 0.0f;
  const ADS1115::Status lastErr = device.lastError();
  const ADS1115::DriverState st = device.state();
  const bool online = device.isOnline();

  Serial.println("=== Driver Health ===");
  Serial.printf("  State: %s%s%s\n",
                stateColor(st, online, device.consecutiveFailures()),
                stateToStr(st),
                LOG_COLOR_RESET);
  Serial.printf("  Online: %s%s%s\n",
                online ? LOG_COLOR_GREEN : LOG_COLOR_RED,
                log_bool_str(online),
                LOG_COLOR_RESET);
  Serial.printf("  Consecutive failures: %s%u%s\n",
                goodIfZeroColor(device.consecutiveFailures()),
                device.consecutiveFailures(),
                LOG_COLOR_RESET);
  Serial.printf("  Total success: %s%lu%s\n",
                goodIfNonZeroColor(totalOk),
                static_cast<unsigned long>(totalOk),
                LOG_COLOR_RESET);
  Serial.printf("  Total failures: %s%lu%s\n",
                goodIfZeroColor(totalFail),
                static_cast<unsigned long>(totalFail),
                LOG_COLOR_RESET);
  Serial.printf("  Success rate: %s%.1f%%%s\n",
                successRateColor(successRate),
                successRate,
                LOG_COLOR_RESET);

  const uint32_t lastOkMs = device.lastOkMs();
  if (lastOkMs > 0U) {
    Serial.printf("  Last OK: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_GREEN,
                  static_cast<unsigned long>(now - lastOkMs),
                  static_cast<unsigned long>(lastOkMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last OK: %snever%s\n", staleTimeColor(false), LOG_COLOR_RESET);
  }

  const uint32_t lastErrorMs = device.lastErrorMs();
  if (lastErrorMs > 0U) {
    Serial.printf("  Last error: %s%lu ms ago (at %lu ms)%s\n",
                  LOG_COLOR_RED,
                  static_cast<unsigned long>(now - lastErrorMs),
                  static_cast<unsigned long>(lastErrorMs),
                  LOG_COLOR_RESET);
  } else {
    Serial.printf("  Last error: %snever%s\n", staleTimeColor(true), LOG_COLOR_RESET);
  }

  if (!lastErr.ok()) {
    Serial.printf("  Error code: %s%s%s\n",
                  LOG_COLOR_RED,
                  errToStr(lastErr.code),
                  LOG_COLOR_RESET);
    Serial.printf("  Error detail: %ld\n", static_cast<long>(lastErr.detail));
    if (lastErr.msg && lastErr.msg[0]) {
      Serial.printf("  Error msg: %s%s%s\n", LOG_COLOR_YELLOW, lastErr.msg, LOG_COLOR_RESET);
    }
  }
}

void printHelp() {
  Serial.println();
  cli::printHelpHeader("ADS1115 CLI Help");
  cli::printHelpSection("Common");
  cli::printHelpItem("help / ?", "Show this help");
  cli::printHelpItem("version / ver", "Print firmware and library version info");
  cli::printHelpItem("scan", "Scan I2C bus");
  cli::printHelpItem("read", "Read single conversion (blocking)");
  cli::printHelpItem("readv", "Read single conversion as voltage (blocking)");
  cli::printHelpItem("read N", "Read N conversions");
  cli::printHelpItem("start", "Start single-shot conversion");
  cli::printHelpItem("poll", "Check if conversion ready");
  cli::printHelpItem("raw", "Read raw value");
  cli::printHelpItem("voltage", "Read as voltage");
  cli::printHelpItem("timing", "Print conversion time and LSB voltage");

  cli::printHelpSection("Configuration");
  cli::printHelpItem("ch [0|1|2|3]", "Set single-ended channel (AINx vs GND)");
  cli::printHelpItem("diff [0|1|2|3]", "Set differential pair");
  cli::printHelpItem("gain [0..5]", "Set PGA (0=6.144V, 2=2.048V, 5=0.256V)");
  cli::printHelpItem("rate [0..7]", "Set data rate");
  cli::printHelpItem("mode [single|cont]", "Set operating mode");
  cli::printHelpItem("comp", "Show comparator config");
  cli::printHelpItem("comp mode [trad|window]", "Set comparator mode");
  cli::printHelpItem("comp pol [low|high]", "Set comparator polarity");
  cli::printHelpItem("comp latch [0|1]", "Set comparator latch");
  cli::printHelpItem("comp queue [1|2|4|disable]", "Set comparator queue");
  cli::printHelpItem("comp th <low> <high>", "Set comparator thresholds");
  cli::printHelpItem("comp rdy", "Enable conversion-ready pin mode");
  cli::printHelpItem("comp disable", "Disable comparator");
  cli::printHelpItem("config", "Dump config register");
  cli::printHelpItem("config write <hex>", "Write full config register value");

  cli::printHelpSection("Registers");
  cli::printHelpItem("reg <0..3>", "Read 16-bit ADS1115 register");
  cli::printHelpItem("wreg <1..3> <val>", "Write writable register (diagnostic only; may desync cached config)");

  cli::printHelpSection("Diagnostics");
  cli::printHelpItem("drv", "Show driver state and health");
  cli::printHelpItem("state", "Show compact one-line health summary");
  cli::printHelpItem("probe", "Probe device (no health tracking)");
  cli::printHelpItem("recover", "Manual recovery attempt");
  cli::printHelpItem("cfg / settings", "Print active configuration snapshot");
  cli::printHelpItem("verbose [0|1]", "Enable/disable verbose output");
  cli::printHelpItem("stress [N]", "Run N conversion cycles");
  cli::printHelpItem("stress_mix [N]", "Run N mixed-operation stress cycles");
  cli::printHelpItem("selftest", "Run safe command self-test report");
}

void printVersionInfo() {
  Serial.println("=== Version Info ===");
  Serial.printf("  Example firmware build: %s %s\n", __DATE__, __TIME__);
  Serial.printf("  ADS1115 library version: %s\n", ADS1115::VERSION);
  Serial.printf("  ADS1115 library full: %s\n", ADS1115::VERSION_FULL);
  Serial.printf("  ADS1115 library build: %s\n", ADS1115::BUILD_TIMESTAMP);
  Serial.printf("  ADS1115 library commit: %s (%s)\n",
                ADS1115::GIT_COMMIT,
                ADS1115::GIT_STATUS);
  Serial.printf("  ADS1115 version code: %d (major=%d minor=%d patch=%d)\n",
                ADS1115::VERSION_INT,
                ADS1115::VERSION_MAJOR,
                ADS1115::VERSION_MINOR,
                ADS1115::VERSION_PATCH);
}

ADS1115::Mux channelToMux(int channel) {
  switch (channel) {
    case 0: return ADS1115::Mux::AIN0_GND;
    case 1: return ADS1115::Mux::AIN1_GND;
    case 2: return ADS1115::Mux::AIN2_GND;
    case 3: return ADS1115::Mux::AIN3_GND;
    default: return ADS1115::Mux::AIN0_GND;
  }
}

ADS1115::Mux diffToMux(int index) {
  switch (index) {
    case 0: return ADS1115::Mux::AIN0_AIN1;
    case 1: return ADS1115::Mux::AIN0_AIN3;
    case 2: return ADS1115::Mux::AIN1_AIN3;
    case 3: return ADS1115::Mux::AIN2_AIN3;
    default: return ADS1115::Mux::AIN0_AIN1;
  }
}

const char* muxToStr(ADS1115::Mux mux) {
  using ADS1115::Mux;
  switch (mux) {
    case Mux::AIN0_AIN1: return "AIN0_AIN1";
    case Mux::AIN0_AIN3: return "AIN0_AIN3";
    case Mux::AIN1_AIN3: return "AIN1_AIN3";
    case Mux::AIN2_AIN3: return "AIN2_AIN3";
    case Mux::AIN0_GND:  return "AIN0_GND";
    case Mux::AIN1_GND:  return "AIN1_GND";
    case Mux::AIN2_GND:  return "AIN2_GND";
    case Mux::AIN3_GND:  return "AIN3_GND";
    default:             return "UNKNOWN";
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
    default:               return "UNKNOWN";
  }
}

const char* pgaBitsToStr(uint8_t bits) {
  switch (bits) {
    case 0: return "FSR_6_144V";
    case 1: return "FSR_4_096V";
    case 2: return "FSR_2_048V";
    case 3: return "FSR_1_024V";
    case 4: return "FSR_0_512V";
    case 5: return "FSR_0_256V";
    case 6: return "FSR_0_256V_ALIAS";
    case 7: return "FSR_0_256V_ALIAS";
    default: return "UNKNOWN";
  }
}

const char* rateToStr(ADS1115::DataRate rate) {
  using ADS1115::DataRate;
  switch (rate) {
    case DataRate::SPS_8:   return "SPS_8";
    case DataRate::SPS_16:  return "SPS_16";
    case DataRate::SPS_32:  return "SPS_32";
    case DataRate::SPS_64:  return "SPS_64";
    case DataRate::SPS_128: return "SPS_128";
    case DataRate::SPS_250: return "SPS_250";
    case DataRate::SPS_475: return "SPS_475";
    case DataRate::SPS_860: return "SPS_860";
    default:                return "UNKNOWN";
  }
}

const char* modeToStr(ADS1115::Mode mode) {
  using ADS1115::Mode;
  switch (mode) {
    case Mode::SINGLE_SHOT: return "SINGLE_SHOT";
    case Mode::CONTINUOUS:  return "CONTINUOUS";
    default:                return "UNKNOWN";
  }
}

const char* compModeToStr(ADS1115::ComparatorMode mode) {
  using ADS1115::ComparatorMode;
  switch (mode) {
    case ComparatorMode::TRADITIONAL: return "TRADITIONAL";
    case ComparatorMode::WINDOW:      return "WINDOW";
    default:                          return "UNKNOWN";
  }
}

const char* compPolToStr(ADS1115::ComparatorPolarity polarity) {
  using ADS1115::ComparatorPolarity;
  switch (polarity) {
    case ComparatorPolarity::ACTIVE_LOW:  return "ACTIVE_LOW";
    case ComparatorPolarity::ACTIVE_HIGH: return "ACTIVE_HIGH";
    default:                              return "UNKNOWN";
  }
}

const char* compLatchToStr(ADS1115::ComparatorLatch latch) {
  using ADS1115::ComparatorLatch;
  switch (latch) {
    case ComparatorLatch::NON_LATCHING: return "NON_LATCHING";
    case ComparatorLatch::LATCHING:     return "LATCHING";
    default:                            return "UNKNOWN";
  }
}

const char* compQueueToStr(ADS1115::ComparatorQueue queue) {
  using ADS1115::ComparatorQueue;
  switch (queue) {
    case ComparatorQueue::ASSERT_1: return "ASSERT_1";
    case ComparatorQueue::ASSERT_2: return "ASSERT_2";
    case ComparatorQueue::ASSERT_4: return "ASSERT_4";
    case ComparatorQueue::DISABLE:  return "DISABLE";
    default:                        return "UNKNOWN";
  }
}

bool parseI32(const String& token, int32_t& out) {
  char* end = nullptr;
  const long value = strtol(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<int32_t>(value);
  return true;
}

bool parseU32(const String& token, uint32_t& out) {
  char* end = nullptr;
  const unsigned long value = strtoul(token.c_str(), &end, 0);
  if (end == token.c_str() || *end != '\0') {
    return false;
  }
  out = static_cast<uint32_t>(value);
  return true;
}

bool parseBool01(const String& token, bool& out) {
  int32_t value = 0;
  if (!parseI32(token, value) || (value != 0 && value != 1)) {
    return false;
  }
  out = (value != 0);
  return true;
}

bool restoreStressBaseline(const ADS1115::SettingsSnapshot& baseline,
                           ADS1115::Status& failure) {
  failure = device.setMode(baseline.mode);
  if (!failure.ok()) {
    return false;
  }
  failure = device.setMux(baseline.mux);
  if (!failure.ok()) {
    return false;
  }
  failure = device.setGain(baseline.gain);
  if (!failure.ok()) {
    return false;
  }
  failure = device.setDataRate(baseline.dataRate);
  return failure.ok();
}

void printComparatorSettings() {
  int16_t low = 0;
  int16_t high = 0;
  ADS1115::Status st = device.getThresholds(low, high);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  Serial.println("=== Comparator ===");
  Serial.printf("  Mode: %s\n", compModeToStr(device.getComparatorMode()));
  Serial.printf("  Polarity: %s\n", compPolToStr(device.getComparatorPolarity()));
  Serial.printf("  Latch: %s\n", compLatchToStr(device.getComparatorLatch()));
  Serial.printf("  Queue: %s\n", compQueueToStr(device.getComparatorQueue()));
  Serial.printf("  Threshold low/high: %d / %d\n", static_cast<int>(low), static_cast<int>(high));
  Serial.printf("  Conversion-ready mode: %s\n",
                device.isConversionReadyModeEnabled() ? "YES" : "NO");
  Serial.printf("  ALERT/RDY pin configured: %s\n",
                device.isAlertRdyPinConfigured() ? "YES" : "NO");
  Serial.printf("  Using ALERT/RDY pin: %s\n",
                device.usesAlertRdyPinForConversionReady() ? "YES" : "NO");
}

void printTimingInfo() {
  Serial.println("=== Timing/Scale ===");
  Serial.printf("  Conversion time: %lu ms\n",
                static_cast<unsigned long>(device.getConversionTimeMs()));
  Serial.printf("  LSB voltage: %.9f V\n", device.getLsbVoltage());
}

bool readConfigFromDevice(uint16_t& config);

bool muxToChannel(ADS1115::Mux mux, int& channel) {
  switch (mux) {
    case ADS1115::Mux::AIN0_GND: channel = 0; return true;
    case ADS1115::Mux::AIN1_GND: channel = 1; return true;
    case ADS1115::Mux::AIN2_GND: channel = 2; return true;
    case ADS1115::Mux::AIN3_GND: channel = 3; return true;
    default: channel = -1; return false;
  }
}

bool muxToDiffIndex(ADS1115::Mux mux, int& index) {
  switch (mux) {
    case ADS1115::Mux::AIN0_AIN1: index = 0; return true;
    case ADS1115::Mux::AIN0_AIN3: index = 1; return true;
    case ADS1115::Mux::AIN1_AIN3: index = 2; return true;
    case ADS1115::Mux::AIN2_AIN3: index = 3; return true;
    default: index = -1; return false;
  }
}

void printCurrentMux() {
  ADS1115::Mux mux = device.getMux();
  int channel = -1;
  int diff = -1;
  if (muxToChannel(mux, channel)) {
    Serial.printf("  Mux: %s (ch %d)\n", muxToStr(mux), channel);
  } else if (muxToDiffIndex(mux, diff)) {
    Serial.printf("  Mux: %s (diff %d)\n", muxToStr(mux), diff);
  } else {
    Serial.printf("  Mux: %s\n", muxToStr(mux));
  }
}

void printCurrentGain() {
  ADS1115::Gain gain = device.getGain();
  Serial.printf("  Gain: %u (%s)\n", static_cast<unsigned>(gain), gainToStr(gain));
}

void printCurrentRate() {
  ADS1115::DataRate rate = device.getDataRate();
  Serial.printf("  Rate: %u (%s)\n", static_cast<unsigned>(rate), rateToStr(rate));
}

void printCurrentMode() {
  ADS1115::Mode mode = device.getMode();
  Serial.printf("  Mode: %s\n", modeToStr(mode));
}

void printConfig() {
  uint16_t config = 0;
  ADS1115::Status st = device.readConfig(config);
  if (!st.ok()) {
    printStatus(st);
    return;
  }
  Serial.printf("  Config: 0x%04X\n", config);
  const uint8_t os = static_cast<uint8_t>((config & ADS1115::cmd::MASK_OS) >> ADS1115::cmd::BIT_OS);
  const uint8_t mux = static_cast<uint8_t>((config & ADS1115::cmd::MASK_MUX) >> ADS1115::cmd::BIT_MUX);
  const uint8_t pga = static_cast<uint8_t>((config & ADS1115::cmd::MASK_PGA) >> ADS1115::cmd::BIT_PGA);
  const uint8_t mode = static_cast<uint8_t>((config & ADS1115::cmd::MASK_MODE) >> ADS1115::cmd::BIT_MODE);
  const uint8_t rate = static_cast<uint8_t>((config & ADS1115::cmd::MASK_DR) >> ADS1115::cmd::BIT_DR);
  const uint8_t compMode = static_cast<uint8_t>((config & ADS1115::cmd::MASK_COMP_MODE) >> ADS1115::cmd::BIT_COMP_MODE);
  const uint8_t compPol = static_cast<uint8_t>((config & ADS1115::cmd::MASK_COMP_POL) >> ADS1115::cmd::BIT_COMP_POL);
  const uint8_t compLatch = static_cast<uint8_t>((config & ADS1115::cmd::MASK_COMP_LAT) >> ADS1115::cmd::BIT_COMP_LAT);
  const uint8_t compQueue = static_cast<uint8_t>((config & ADS1115::cmd::MASK_COMP_QUE) >> ADS1115::cmd::BIT_COMP_QUE);
  Serial.printf("  Fields: OS=%u(%s) MUX=%s(%u) PGA=%s(%u) MODE=%s DR=%s(%u)\n",
                os,
                os ? "idle/start" : "busy",
                muxToStr(static_cast<ADS1115::Mux>(mux)),
                mux,
                pgaBitsToStr(pga),
                pga,
                modeToStr(static_cast<ADS1115::Mode>(mode)),
                rateToStr(static_cast<ADS1115::DataRate>(rate)),
                rate);
  Serial.printf("  Comparator: mode=%s polarity=%s latch=%s queue=%s\n",
                compModeToStr(static_cast<ADS1115::ComparatorMode>(compMode)),
                compPolToStr(static_cast<ADS1115::ComparatorPolarity>(compPol)),
                compLatchToStr(static_cast<ADS1115::ComparatorLatch>(compLatch)),
                compQueueToStr(static_cast<ADS1115::ComparatorQueue>(compQueue)));
}

void printSettingsSnapshot() {
  ADS1115::SettingsSnapshot snap;
  ADS1115::Status st = device.getSettings(snap);
  if (!st.ok()) {
    printStatus(st);
    return;
  }

  Serial.println("=== Cached Settings ===");
  Serial.printf("  Initialized: %s\n", snap.initialized ? "YES" : "NO");
  Serial.printf("  State: %s\n", stateToStr(snap.state));
  Serial.printf("  Address: 0x%02X\n", snap.i2cAddress);
  Serial.printf("  Timeout: %lu ms\n", static_cast<unsigned long>(snap.i2cTimeoutMs));
  Serial.printf("  Offline threshold: %u\n", static_cast<unsigned>(snap.offlineThreshold));
  Serial.printf("  Hooks: now=%s gpio=%s yield=%s\n",
                snap.hasNowMsHook ? "YES" : "NO",
                snap.hasGpioReadHook ? "YES" : "NO",
                snap.hasCooperativeYieldHook ? "YES" : "NO");
  Serial.printf("  Alert pin: %d\n", snap.alertRdyPin);
  Serial.printf("  ALERT/RDY pin configured: %s\n",
                snap.alertRdyPinConfigured ? "YES" : "NO");
  Serial.printf("  Conversion-ready mode: %s\n",
                snap.conversionReadyModeEnabled ? "YES" : "NO");
  Serial.printf("  Using ALERT/RDY pin: %s\n",
                snap.usesAlertRdyPin ? "YES" : "NO");
  Serial.printf("  Mux: %s\n", muxToStr(snap.mux));
  Serial.printf("  Gain: %s\n", gainToStr(snap.gain));
  Serial.printf("  Rate: %s\n", rateToStr(snap.dataRate));
  Serial.printf("  Mode: %s\n", modeToStr(snap.mode));
  Serial.printf("  Comparator: mode=%s pol=%s latch=%s queue=%s\n",
                compModeToStr(snap.compMode),
                compPolToStr(snap.compPolarity),
                compLatchToStr(snap.compLatch),
                compQueueToStr(snap.compQueue));
  Serial.printf("  Thresholds: low=%d high=%d\n",
                static_cast<int>(snap.compThresholdLow),
                static_cast<int>(snap.compThresholdHigh));
  Serial.printf("  Conversion: started=%s ready=%s start=%lu ms lastRaw=%d\n",
                snap.conversionStarted ? "YES" : "NO",
                snap.conversionReady ? "YES" : "NO",
                static_cast<unsigned long>(snap.conversionStartMs),
                static_cast<int>(snap.lastRawValue));
}

bool readConfigFromDevice(uint16_t& config) {
  ADS1115::Status st = device.readConfig(config);
  if (!st.ok()) {
    printStatus(st);
    return false;
  }
  return true;
}

void runStressMix(int count) {
  struct OpStats {
    const char* name;
    uint32_t ok;
    uint32_t fail;
  };

  OpStats stats[] = {
      {"readBlocking", 0, 0},
      {"readVoltage", 0, 0},
      {"start+readRaw", 0, 0},
      {"readConfig", 0, 0},
      {"setGain", 0, 0},
      {"setRate", 0, 0},
  };
  const int opCount = static_cast<int>(sizeof(stats) / sizeof(stats[0]));

  ADS1115::SettingsSnapshot baseline;
  ADS1115::Status snapshotStatus = device.getSettings(baseline);
  const bool haveBaseline = snapshotStatus.ok();
  ADS1115::Status restoreStatus = ADS1115::Status::Ok();
  ADS1115::Status prepStatus = ADS1115::Status::Ok();
  HealthSnapshot<ADS1115::ADS1115> healthBefore;
  healthBefore.capture(device);
  const uint32_t successBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint32_t startMs = millis();
  uint32_t okTotal = 0;
  uint32_t failTotal = 0;
  bool hasFailure = false;
  ADS1115::Status firstFailure = ADS1115::Status::Ok();
  ADS1115::Status lastFailure = ADS1115::Status::Ok();

  if (haveBaseline && baseline.mode != ADS1115::Mode::SINGLE_SHOT) {
    prepStatus = device.setMode(ADS1115::Mode::SINGLE_SHOT);
  }

  for (int i = 0; i < count; ++i) {
    ADS1115::Status st = prepStatus;
    const int op = i % opCount;

    if (st.ok()) {
      switch (op) {
        case 0: {
          int16_t raw = 0;
          st = device.readBlocking(raw);
          break;
        }
        case 1: {
          float volts = 0.0f;
          st = device.readBlockingVoltage(volts);
          break;
        }
        case 2: {
          st = device.startConversion();
          if (st.ok() || st.inProgress()) {
            const uint32_t waitStartMs = millis();
            bool ready = false;
            while ((millis() - waitStartMs) < 200U) {
              st = device.readConversionReady(ready);
              if (!st.ok()) {
                break;
              }
              if (ready) {
                break;
              }
              device.tick(millis());
            }
            if (st.ok() && !ready) {
              st = ADS1115::Status::Error(ADS1115::Err::TIMEOUT,
                                          "conversion timeout",
                                          200);
            }
            if (st.ok()) {
              int16_t raw = 0;
              st = device.readRaw(raw);
            }
          }
          break;
        }
        case 3: {
          uint16_t cfg = 0;
          st = device.readConfig(cfg);
          break;
        }
        case 4: {
          const ADS1115::Gain gain = static_cast<ADS1115::Gain>(i % 6);
          st = device.setGain(gain);
          break;
        }
        case 5: {
          const ADS1115::DataRate rate = static_cast<ADS1115::DataRate>(i % 8);
          st = device.setDataRate(rate);
          break;
        }
        default:
          break;
      }
    }

    if (st.ok()) {
      stats[op].ok++;
      okTotal++;
    } else {
      stats[op].fail++;
      failTotal++;
      if (!hasFailure) {
        firstFailure = st;
        hasFailure = true;
      }
      lastFailure = st;
      LOGV(verboseMode, "[%d] %s failed: %s", i, stats[op].name, errToStr(st.code));
    }

    printStressProgress(static_cast<uint32_t>(i + 1),
                        static_cast<uint32_t>(count),
                        okTotal,
                        failTotal);

    if ((i + 1) % 50 == 0) {
      device.tick(millis());
    }
  }

  if (haveBaseline) {
    (void)restoreStressBaseline(baseline, restoreStatus);
  }
  const uint32_t elapsed = millis() - startMs;
  HealthSnapshot<ADS1115::ADS1115> healthAfter;
  healthAfter.capture(device);

  Serial.println("=== stress_mix summary ===");
  const float successPct =
      (count > 0) ? (100.0f * static_cast<float>(okTotal) / static_cast<float>(count)) : 0.0f;
  Serial.printf("  Total: %sok=%lu%s %sfail=%lu%s (%s%.2f%%%s)\n",
                goodIfNonZeroColor(okTotal),
                static_cast<unsigned long>(okTotal),
                LOG_COLOR_RESET,
                goodIfZeroColor(failTotal),
                static_cast<unsigned long>(failTotal),
                LOG_COLOR_RESET,
                successRateColor(successPct),
                successPct,
                LOG_COLOR_RESET);
  Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  if (elapsed > 0) {
    Serial.printf("  Rate: %.2f ops/s\n", (1000.0f * static_cast<float>(count)) / elapsed);
  }
  for (int i = 0; i < opCount; ++i) {
    const uint32_t opTotal = stats[i].ok + stats[i].fail;
    const float opPct = (opTotal > 0U)
                            ? (100.0f * static_cast<float>(stats[i].ok) /
                               static_cast<float>(opTotal))
                            : 0.0f;
    Serial.printf("  %-12s %sok=%lu%s %sfail=%lu%s (%s%.1f%%%s)\n",
                  stats[i].name,
                  goodIfNonZeroColor(stats[i].ok),
                  static_cast<unsigned long>(stats[i].ok),
                  LOG_COLOR_RESET,
                  goodIfZeroColor(stats[i].fail),
                  static_cast<unsigned long>(stats[i].fail),
                  LOG_COLOR_RESET,
                  successRateColor(opPct),
                  opPct,
                  LOG_COLOR_RESET);
  }
  const uint32_t successDelta = device.totalSuccess() - successBefore;
  const uint32_t failDelta = device.totalFailures() - failBefore;
  Serial.printf("  Health delta: %ssuccess +%lu%s, %sfailures +%lu%s\n",
                goodIfNonZeroColor(successDelta),
                static_cast<unsigned long>(successDelta),
                LOG_COLOR_RESET,
                goodIfZeroColor(failDelta),
                static_cast<unsigned long>(failDelta),
                LOG_COLOR_RESET);
  Serial.println("  Health changes:");
  printHealthDiff(healthBefore, healthAfter);
  if (!haveBaseline) {
    Serial.printf("  Baseline restore: %sSKIPPED%s (snapshot unavailable)\n",
                  LOG_COLOR_YELLOW,
                  LOG_COLOR_RESET);
  } else if (!restoreStatus.ok()) {
    Serial.printf("  Baseline restore: %sFAILED%s\n", LOG_COLOR_RED, LOG_COLOR_RESET);
    printStatus(restoreStatus);
  } else {
    Serial.printf("  Baseline restore: %sOK%s\n", LOG_COLOR_GREEN, LOG_COLOR_RESET);
  }
  if (!prepStatus.ok()) {
    Serial.println("  Prep failure:");
    printStatus(prepStatus);
  }
  if (hasFailure) {
    Serial.println("  First failure:");
    printStatus(firstFailure);
    if (failTotal > 1U) {
      Serial.println("  Last failure:");
      printStatus(lastFailure);
    }
  }
}

void runStress(int count) {
  HealthSnapshot<ADS1115::ADS1115> healthBefore;
  healthBefore.capture(device);
  const uint32_t successBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint32_t startMs = millis();
  int ok = 0;
  int fail = 0;
  bool hasFailure = false;
  ADS1115::Status firstFailure = ADS1115::Status::Ok();
  ADS1115::Status lastFailure = ADS1115::Status::Ok();

  for (int i = 0; i < count; ++i) {
    int16_t raw = 0;
    ADS1115::Status st = device.readBlocking(raw);
    if (st.ok()) {
      ok++;
      LOGV(verboseMode, "  %d: %d (%.6f V)", i + 1, raw, device.rawToVoltage(raw));
    } else {
      fail++;
      if (!hasFailure) {
        firstFailure = st;
        hasFailure = true;
      }
      lastFailure = st;
      if (verboseMode) {
        printStatus(st);
      }
    }

    printStressProgress(static_cast<uint32_t>(i + 1),
                        static_cast<uint32_t>(count),
                        static_cast<uint32_t>(ok),
                        static_cast<uint32_t>(fail));
  }

  const uint32_t elapsed = millis() - startMs;
  const float pct =
      (count > 0) ? (100.0f * static_cast<float>(ok) / static_cast<float>(count)) : 0.0f;
  const uint32_t successDelta = device.totalSuccess() - successBefore;
  const uint32_t failDelta = device.totalFailures() - failBefore;
  HealthSnapshot<ADS1115::ADS1115> healthAfter;
  healthAfter.capture(device);

  Serial.println("=== Stress Summary ===");
  Serial.printf("  Total: %d\n", count);
  Serial.printf("  Success: %s%d%s\n",
                goodIfNonZeroColor(static_cast<uint32_t>(ok)),
                ok,
                LOG_COLOR_RESET);
  Serial.printf("  Errors: %s%d%s\n",
                goodIfZeroColor(static_cast<uint32_t>(fail)),
                fail,
                LOG_COLOR_RESET);
  Serial.printf("  Success rate: %s%.2f%%%s\n",
                successRateColor(pct),
                pct,
                LOG_COLOR_RESET);
  Serial.printf("  Duration: %lu ms\n", static_cast<unsigned long>(elapsed));
  if (elapsed > 0U) {
    Serial.printf("  Rate: %.2f samples/s\n",
                  (1000.0f * static_cast<float>(count)) / static_cast<float>(elapsed));
  }
  Serial.printf("  Health delta: %ssuccess +%lu%s, %sfailures +%lu%s\n",
                goodIfNonZeroColor(successDelta),
                static_cast<unsigned long>(successDelta),
                LOG_COLOR_RESET,
                goodIfZeroColor(failDelta),
                static_cast<unsigned long>(failDelta),
                LOG_COLOR_RESET);
  Serial.println("  Health changes:");
  printHealthDiff(healthBefore, healthAfter);
  if (hasFailure) {
    Serial.println("  Failure details:");
    Serial.println("  First failure:");
    printStatus(firstFailure);
    if (fail > 1) {
      Serial.println("  Last failure:");
      printStatus(lastFailure);
    }
  }
}

void runSelfTest() {
  struct TestStats {
    uint32_t pass = 0;
    uint32_t fail = 0;
    uint32_t skip = 0;
  } stats;

  enum class SelftestOutcome : uint8_t { PASS, FAIL, SKIP };
  auto report = [&](const char* name, SelftestOutcome outcome, const char* note) {
    const bool passed = (outcome == SelftestOutcome::PASS);
    const bool skipped = (outcome == SelftestOutcome::SKIP);
    const char* color = skipped ? LOG_COLOR_YELLOW : LOG_COLOR_RESULT(passed);
    const char* tag = skipped ? "SKIP" : (passed ? "PASS" : "FAIL");
    Serial.printf("  [%s%s%s] %s", color, tag, LOG_COLOR_RESET, name);
    if (note && note[0]) {
      Serial.printf(" - %s", note);
    }
    Serial.println();
    if (skipped) {
      stats.skip++;
    } else if (passed) {
      stats.pass++;
    } else {
      stats.fail++;
    }
  };
  auto reportCheck = [&](const char* name, bool passed, const char* note) {
    report(name, passed ? SelftestOutcome::PASS : SelftestOutcome::FAIL, note);
  };
  auto reportSkip = [&](const char* name, const char* note) {
    report(name, SelftestOutcome::SKIP, note);
  };

  Serial.println("=== ADS1115 selftest (safe commands) ===");

  const uint32_t succBefore = device.totalSuccess();
  const uint32_t failBefore = device.totalFailures();
  const uint8_t consBefore = device.consecutiveFailures();

  const ADS1115::Status pst = device.probe();
  if (pst.code == ADS1115::Err::NOT_INITIALIZED) {
    reportSkip("probe responds", "driver not initialized");
    reportSkip("remaining checks", "selftest aborted");
    Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                  goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                  goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                  skipCountColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
    return;
  }
  const bool probeHealthUnchanged =
      device.totalSuccess() == succBefore &&
      device.totalFailures() == failBefore &&
      device.consecutiveFailures() == consBefore;
  reportCheck("probe responds", pst.ok(), pst.ok() ? "" : errToStr(pst.code));
  reportCheck("probe no-health-side-effects", probeHealthUnchanged, "");

  uint16_t cfg = 0;
  ADS1115::Status st = device.readConfig(cfg);
  reportCheck("readConfig", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.setMode(ADS1115::Mode::SINGLE_SHOT);
  reportCheck("setMode(single)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok() && device.readConfig(cfg).ok()) {
    const uint16_t modeBits =
        (cfg & ADS1115::cmd::MASK_MODE) >> ADS1115::cmd::BIT_MODE;
    reportCheck("verify mode single", modeBits == static_cast<uint16_t>(ADS1115::Mode::SINGLE_SHOT), "");
  } else {
    reportCheck("verify mode single", false, "readConfig failed");
  }

  st = device.setMode(ADS1115::Mode::CONTINUOUS);
  reportCheck("setMode(continuous)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok() && device.readConfig(cfg).ok()) {
    const uint16_t modeBits =
        (cfg & ADS1115::cmd::MASK_MODE) >> ADS1115::cmd::BIT_MODE;
    reportCheck("verify mode continuous", modeBits == static_cast<uint16_t>(ADS1115::Mode::CONTINUOUS), "");
  } else {
    reportCheck("verify mode continuous", false, "readConfig failed");
  }

  st = device.setMode(ADS1115::Mode::SINGLE_SHOT);
  reportCheck("restore mode(single)", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.setGain(ADS1115::Gain::FSR_2_048V);
  reportCheck("setGain(2.048V)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok() && device.readConfig(cfg).ok()) {
    const uint16_t gainBits =
        (cfg & ADS1115::cmd::MASK_PGA) >> ADS1115::cmd::BIT_PGA;
    reportCheck("verify gain bits", gainBits == static_cast<uint16_t>(ADS1115::Gain::FSR_2_048V), "");
  } else {
    reportCheck("verify gain bits", false, "readConfig failed");
  }

  st = device.setDataRate(ADS1115::DataRate::SPS_128);
  reportCheck("setRate(128sps)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok() && device.readConfig(cfg).ok()) {
    const uint16_t rateBits =
        (cfg & ADS1115::cmd::MASK_DR) >> ADS1115::cmd::BIT_DR;
    reportCheck("verify rate bits", rateBits == static_cast<uint16_t>(ADS1115::DataRate::SPS_128), "");
  } else {
    reportCheck("verify rate bits", false, "readConfig failed");
  }

  st = device.setMux(ADS1115::Mux::AIN0_GND);
  reportCheck("setMux(AIN0_GND)", st.ok(), st.ok() ? "" : errToStr(st.code));
  if (st.ok() && device.readConfig(cfg).ok()) {
    const uint16_t muxBits =
        (cfg & ADS1115::cmd::MASK_MUX) >> ADS1115::cmd::BIT_MUX;
    reportCheck("verify mux bits", muxBits == static_cast<uint16_t>(ADS1115::Mux::AIN0_GND), "");
  } else {
    reportCheck("verify mux bits", false, "readConfig failed");
  }

  st = device.startConversion();
  const bool started = st.ok() || st.inProgress();
  reportCheck("startConversion", started, started ? "" : errToStr(st.code));
  if (started) {
    const uint32_t waitStartMs = millis();
    bool ready = false;
    ADS1115::Status rs = ADS1115::Status::Ok();
    while ((millis() - waitStartMs) < 200U) {
      rs = device.readConversionReady(ready);
      if (!rs.ok() || ready) {
        break;
      }
      device.tick(millis());
    }
    reportCheck("poll after start", rs.ok() && ready, rs.ok() ? "" : errToStr(rs.code));
    int16_t raw = 0;
    rs = device.readRaw(raw);
    reportCheck("readRaw(after start)", rs.ok(), rs.ok() ? "" : errToStr(rs.code));
  } else {
    reportCheck("poll after start", false, "conversion not started");
    reportCheck("readRaw(after start)", false, "conversion not started");
  }

  float volts = 0.0f;
  st = device.readVoltage(volts);
  reportCheck("readVoltage", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.readBlockingVoltage(volts);
  reportCheck("readBlockingVoltage", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.setComparatorMode(ADS1115::ComparatorMode::TRADITIONAL);
  reportCheck("setComparatorMode", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorPolarity(ADS1115::ComparatorPolarity::ACTIVE_LOW);
  reportCheck("setComparatorPolarity", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorLatch(ADS1115::ComparatorLatch::NON_LATCHING);
  reportCheck("setComparatorLatch", st.ok(), st.ok() ? "" : errToStr(st.code));
  st = device.setComparatorQueue(ADS1115::ComparatorQueue::DISABLE);
  reportCheck("setComparatorQueue", st.ok(), st.ok() ? "" : errToStr(st.code));

  int16_t low = 0;
  int16_t high = 0;
  st = device.getThresholds(low, high);
  reportCheck("getThresholds", st.ok(), st.ok() ? "" : errToStr(st.code));

  st = device.recover();
  reportCheck("recover", st.ok(), st.ok() ? "" : errToStr(st.code));
  reportCheck("isOnline", device.isOnline(), "");

  Serial.printf("Selftest result: pass=%s%lu%s fail=%s%lu%s skip=%s%lu%s\n",
                goodIfNonZeroColor(stats.pass), static_cast<unsigned long>(stats.pass), LOG_COLOR_RESET,
                goodIfZeroColor(stats.fail), static_cast<unsigned long>(stats.fail), LOG_COLOR_RESET,
                skipCountColor(stats.skip), static_cast<unsigned long>(stats.skip), LOG_COLOR_RESET);
}

// ============================================================================
// Command Processing
// ============================================================================

void processCommand(const String& cmdLine) {
  String cmd = cmdLine;
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  if (cmd == "help" || cmd == "?") {
    printHelp();
  } else if (cmd == "version" || cmd == "ver") {
    printVersionInfo();
  } else if (cmd == "scan") {
    bus_diag::scan();
  } else if (cmd == "state") {
    printHealthView(device);
  } else if (cmd == "probe") {
    LOGI("Probing device (no health tracking)...");
    HealthSnapshot<ADS1115::ADS1115> before;
    before.capture(device);
    auto st = device.probe();
    printStatus(st);
    HealthSnapshot<ADS1115::ADS1115> after;
    after.capture(device);
    Serial.println("  Health changes:");
    printHealthDiff(before, after);
  } else if (cmd == "drv") {
    printDriverHealth();
  } else if (cmd == "recover") {
    LOGI("Attempting recovery...");
    HealthSnapshot<ADS1115::ADS1115> before;
    before.capture(device);
    auto st = device.recover();
    printStatus(st);
    HealthSnapshot<ADS1115::ADS1115> after;
    after.capture(device);
    Serial.println("  Health changes:");
    printHealthDiff(before, after);
    printDriverHealth();
  } else if (cmd == "verbose") {
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
  } else if (cmd.startsWith("verbose ")) {
    bool value = false;
    if (!parseBool01(cmd.substring(8), value)) {
      LOGW("Usage: verbose [0|1]");
      return;
    }
    verboseMode = value;
    LOGI("Verbose mode: %s%s%s", onOffColor(verboseMode), verboseMode ? "ON" : "OFF", LOG_COLOR_RESET);
  } else if (cmd == "start") {
    auto st = device.startConversion();
    printStatus(st);
  } else if (cmd == "poll") {
    bool ready = false;
    auto st = device.readConversionReady(ready);
    if (st.ok()) {
      LOGI("Conversion ready: %s%s%s", yesNoColor(ready), ready ? "YES" : "NO", LOG_COLOR_RESET);
    } else {
      printStatus(st);
    }
  } else if (cmd == "raw") {
    int16_t raw = 0;
    auto st = device.readRaw(raw);
    if (st.ok()) {
      Serial.printf("  Raw: %d\n", raw);
      LOGV(verboseMode, "  Voltage: %.6f V", device.rawToVoltage(raw));
    } else {
      printStatus(st);
    }
  } else if (cmd == "voltage") {
    float volts = 0.0f;
    auto st = device.readVoltage(volts);
    if (st.ok()) {
      Serial.printf("  Voltage: %.6f V\n", volts);
    } else {
      printStatus(st);
    }
  } else if (cmd == "readv") {
    float volts = 0.0f;
    auto st = device.readBlockingVoltage(volts);
    if (st.ok()) {
      Serial.printf("  Blocking voltage: %.6f V\n", volts);
    } else {
      printStatus(st);
    }
  } else if (cmd == "read") {
    int16_t raw = 0;
    auto st = device.readBlocking(raw);
    if (st.ok()) {
      Serial.printf("  Raw: %d\n", raw);
      Serial.printf("  Voltage: %.6f V\n", device.rawToVoltage(raw));
    } else {
      printStatus(st);
    }
  } else if (cmd.startsWith("read ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(5), count) || count <= 0 || count > 10000) {
      LOGW("Invalid count (1-10000)");
      return;
    }
    for (int32_t i = 0; i < count; ++i) {
      int16_t raw = 0;
      auto st = device.readBlocking(raw);
      if (!st.ok()) {
        printStatus(st);
        break;
      }
      Serial.printf("  %ld: %d (%.6f V)\n",
                    static_cast<long>(i + 1),
                    raw,
                    device.rawToVoltage(raw));
    }
  } else if (cmd == "ch") {
    printCurrentMux();
  } else if (cmd.startsWith("ch ")) {
    int32_t channel = 0;
    if (!parseI32(cmd.substring(3), channel)) {
      LOGW("Invalid channel");
      return;
    }
    if (channel < 0 || channel > 3) {
      LOGW("Invalid channel");
      return;
    }
    auto st = device.setMux(channelToMux(static_cast<int>(channel)));
    printStatus(st);
  } else if (cmd == "diff") {
    printCurrentMux();
  } else if (cmd.startsWith("diff ")) {
    int32_t idx = 0;
    if (!parseI32(cmd.substring(5), idx)) {
      LOGW("Invalid differential index");
      return;
    }
    if (idx < 0 || idx > 3) {
      LOGW("Invalid differential index");
      return;
    }
    auto st = device.setMux(diffToMux(static_cast<int>(idx)));
    printStatus(st);
  } else if (cmd == "gain") {
    printCurrentGain();
  } else if (cmd.startsWith("gain ")) {
    int32_t gain = 0;
    if (!parseI32(cmd.substring(5), gain)) {
      LOGW("Invalid gain");
      return;
    }
    if (gain < 0 || gain > 5) {
      LOGW("Invalid gain");
      return;
    }
    auto st = device.setGain(static_cast<ADS1115::Gain>(gain));
    printStatus(st);
  } else if (cmd == "rate") {
    printCurrentRate();
  } else if (cmd.startsWith("rate ")) {
    int32_t rate = 0;
    if (!parseI32(cmd.substring(5), rate)) {
      LOGW("Invalid rate");
      return;
    }
    if (rate < 0 || rate > 7) {
      LOGW("Invalid rate");
      return;
    }
    auto st = device.setDataRate(static_cast<ADS1115::DataRate>(rate));
    printStatus(st);
  } else if (cmd == "mode") {
    printCurrentMode();
  } else if (cmd.startsWith("mode ")) {
    String mode = cmd.substring(5);
    mode.trim();
    if (mode == "single") {
      auto st = device.setMode(ADS1115::Mode::SINGLE_SHOT);
      printStatus(st);
    } else if (mode == "cont" || mode == "continuous") {
      auto st = device.setMode(ADS1115::Mode::CONTINUOUS);
      printStatus(st);
    } else {
      LOGW("Invalid mode");
    }
  } else if (cmd == "timing") {
    printTimingInfo();
  } else if (cmd == "comp") {
    printComparatorSettings();
  } else if (cmd.startsWith("comp mode ")) {
    String token = cmd.substring(10);
    token.trim();
    ADS1115::ComparatorMode mode = ADS1115::ComparatorMode::TRADITIONAL;
    if (token == "trad" || token == "traditional") {
      mode = ADS1115::ComparatorMode::TRADITIONAL;
    } else if (token == "window") {
      mode = ADS1115::ComparatorMode::WINDOW;
    } else {
      LOGW("Usage: comp mode [trad|window]");
      return;
    }
    printStatus(device.setComparatorMode(mode));
  } else if (cmd.startsWith("comp pol ")) {
    String token = cmd.substring(9);
    token.trim();
    ADS1115::ComparatorPolarity polarity = ADS1115::ComparatorPolarity::ACTIVE_LOW;
    if (token == "low" || token == "active_low") {
      polarity = ADS1115::ComparatorPolarity::ACTIVE_LOW;
    } else if (token == "high" || token == "active_high") {
      polarity = ADS1115::ComparatorPolarity::ACTIVE_HIGH;
    } else {
      LOGW("Usage: comp pol [low|high]");
      return;
    }
    printStatus(device.setComparatorPolarity(polarity));
  } else if (cmd.startsWith("comp latch ")) {
    int32_t val = 0;
    if (!parseI32(cmd.substring(11), val)) {
      LOGW("Usage: comp latch [0|1]");
      return;
    }
    if (val != 0 && val != 1) {
      LOGW("Usage: comp latch [0|1]");
      return;
    }
    const ADS1115::ComparatorLatch latch =
        (val == 0) ? ADS1115::ComparatorLatch::NON_LATCHING
                   : ADS1115::ComparatorLatch::LATCHING;
    printStatus(device.setComparatorLatch(latch));
  } else if (cmd.startsWith("comp queue ")) {
    String token = cmd.substring(11);
    token.trim();
    ADS1115::ComparatorQueue queue = ADS1115::ComparatorQueue::DISABLE;
    if (token == "1") {
      queue = ADS1115::ComparatorQueue::ASSERT_1;
    } else if (token == "2") {
      queue = ADS1115::ComparatorQueue::ASSERT_2;
    } else if (token == "4") {
      queue = ADS1115::ComparatorQueue::ASSERT_4;
    } else if (token == "disable" || token == "off") {
      queue = ADS1115::ComparatorQueue::DISABLE;
    } else {
      LOGW("Usage: comp queue [1|2|4|disable]");
      return;
    }
    printStatus(device.setComparatorQueue(queue));
  } else if (cmd.startsWith("comp th ")) {
    String args = cmd.substring(8);
    args.trim();
    const int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: comp th <low> <high>");
      return;
    }
    String lowTok = args.substring(0, split);
    String highTok = args.substring(split + 1);
    lowTok.trim();
    highTok.trim();
    int32_t low = 0;
    int32_t high = 0;
    if (!parseI32(lowTok, low) || !parseI32(highTok, high) ||
        low < -32768 || low > 32767 || high < -32768 || high > 32767) {
      LOGW("Thresholds must be in int16 range");
      return;
    }
    printStatus(device.setThresholds(static_cast<int16_t>(low), static_cast<int16_t>(high)));
  } else if (cmd == "comp rdy") {
    printStatus(device.enableConversionReadyPin());
  } else if (cmd == "comp disable") {
    printStatus(device.disableComparator());
  } else if (cmd.startsWith("config write ")) {
    uint32_t value = 0;
    String token = cmd.substring(13);
    token.trim();
    if (!parseU32(token, value) || value > 0xFFFFu) {
      LOGW("Usage: config write <0..0xFFFF>");
      return;
    }
    ADS1115::Status st = device.writeConfig(static_cast<uint16_t>(value));
    printStatus(st);
    if (st.ok()) {
      printConfig();
    }
  } else if (cmd.startsWith("wreg ")) {
    String args = cmd.substring(5);
    args.trim();
    const int split = args.indexOf(' ');
    if (split < 0) {
      LOGW("Usage: wreg <1..3> <val>");
      return;
    }

    uint32_t addr = 0;
    uint32_t value = 0;
    if (!parseU32(args.substring(0, split), addr) ||
        !parseU32(args.substring(split + 1), value) ||
        addr < ADS1115::cmd::REG_CONFIG ||
        addr > ADS1115::cmd::REG_HI_THRESH ||
        value > 0xFFFFu) {
      LOGW("Usage: wreg <1..3> <val>");
      return;
    }

    printStatus(device.writeRegister16(static_cast<uint8_t>(addr), static_cast<uint16_t>(value)));
  } else if (cmd.startsWith("reg ")) {
    uint32_t addr = 0;
    if (!parseU32(cmd.substring(4), addr) || addr > ADS1115::cmd::REG_HI_THRESH) {
      LOGW("Usage: reg <0..3>");
      return;
    }

    uint16_t value = 0;
    ADS1115::Status st = device.readRegister16(static_cast<uint8_t>(addr), value);
    if (!st.ok()) {
      printStatus(st);
      return;
    }

    Serial.printf("  Reg 0x%02lX = 0x%04X (%u)\n",
                  static_cast<unsigned long>(addr),
                  value,
                  value);
  } else if (cmd == "selftest") {
    runSelfTest();
  } else if (cmd == "stress_mix") {
    runStressMix(50);
  } else if (cmd.startsWith("stress_mix ")) {
    int32_t count = 0;
    if (!parseI32(cmd.substring(11), count)) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    if (count <= 0 || count > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    runStressMix(count);
  } else if (cmd.startsWith("stress")) {
    int32_t count = 10;
    if (cmd.length() > 6) {
      if (!parseI32(cmd.substring(7), count)) {
        LOGW("Invalid count (1-100000)");
        return;
      }
    }
    if (count <= 0 || count > 100000) {
      LOGW("Invalid count (1-100000)");
      return;
    }
    runStress(count);
  } else if (cmd == "config" || cmd == "cfg" || cmd == "settings") {
    printConfig();
    printSettingsSnapshot();
  } else {
    LOGW("Unknown command: %s", cmd.c_str());
  }
}

// ============================================================================
// Setup and Loop
// ============================================================================

void setup() {
  board::initSerial();
  delay(100);

  LOGI("=== ADS1115 Bringup Example ===");

  if (!board::initI2c()) {
    LOGE("Failed to initialize I2C");
    return;
  }
  LOGI("I2C initialized (SDA=%d, SCL=%d)", board::I2C_SDA, board::I2C_SCL);

  board::initAlertRdyPin();

  bus_diag::scan();

  ADS1115::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cUser = transport::configUser();
  cfg.nowMs = transport::arduinoNowMs;
  cfg.cooperativeYield = transport::arduinoYield;
  cfg.i2cAddress = board::ADS1115_I2C_ADDR;
  cfg.i2cTimeoutMs = board::I2C_TIMEOUT_MS;
  cfg.offlineThreshold = 5;
  if (board::ALERT_RDY_PIN >= 0) {
    cfg.alertRdyPin = board::ALERT_RDY_PIN;
    cfg.gpioRead = board::readAlertRdyPin;
  }

  auto st = device.begin(cfg);
  if (!st.ok()) {
    LOGE("Failed to initialize device");
    printStatus(st);
    return;
  }

  LOGI("Device initialized successfully");
  printDriverHealth();

  Serial.println("\nType 'help' for commands");
  cli::printPrompt();
}

void loop() {
  device.tick(millis());

  static String inputBuffer;
  static constexpr size_t kMaxInputLen = 128;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
        cli::printPrompt();
      }
    } else if (inputBuffer.length() < kMaxInputLen) {
      inputBuffer += c;
    }
  }
}
