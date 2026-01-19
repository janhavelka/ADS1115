/// @file main.cpp
/// @brief ADS1115 basic bringup example
/// @note This is an EXAMPLE, not part of the library

#include <Arduino.h>

#include "examples/common/BoardConfig.h"
#include "examples/common/I2cScanner.h"
#include "examples/common/I2cTransport.h"
#include "examples/common/Log.h"

#include "ADS1115/ADS1115.h"

// ============================================================================
// Globals
// ============================================================================

ADS1115::ADS1115 device;
bool verboseMode = false;

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

void printStatus(const ADS1115::Status& st) {
  Serial.printf("  Status: %s (code=%u, detail=%ld)\n",
                errToStr(st.code),
                static_cast<unsigned>(st.code),
                static_cast<long>(st.detail));
  if (st.msg && st.msg[0]) {
    Serial.printf("  Message: %s\n", st.msg);
  }
}

void printDriverHealth() {
  Serial.println("=== Driver State ===");
  Serial.printf("  State: %s\n", stateToStr(device.state()));
  Serial.printf("  Consecutive failures: %u\n", device.consecutiveFailures());
  Serial.printf("  Total failures: %lu\n", static_cast<unsigned long>(device.totalFailures()));
  Serial.printf("  Total success: %lu\n", static_cast<unsigned long>(device.totalSuccess()));
  Serial.printf("  Last OK at: %lu ms\n", static_cast<unsigned long>(device.lastOkMs()));
  Serial.printf("  Last error at: %lu ms\n", static_cast<unsigned long>(device.lastErrorMs()));
  if (device.lastError().code != ADS1115::Err::OK) {
    Serial.printf("  Last error: %s\n", errToStr(device.lastError().code));
  }
}

void printHelp() {
  Serial.println("Commands:");
  Serial.println("  help              - Show this help");
  Serial.println("  read              - Read single conversion (blocking)");
  Serial.println("  read N            - Read N conversions");
  Serial.println("  start             - Start single-shot conversion");
  Serial.println("  poll              - Check if conversion ready");
  Serial.println("  raw               - Read raw value");
  Serial.println("  voltage           - Read as voltage");
  Serial.println();
  Serial.println("Channel/Gain:");
  Serial.println("  ch [0|1|2|3]      - Set single-ended channel (AINx vs GND)");
  Serial.println("  diff [0|1|2|3]    - Set differential pair");
  Serial.println("  gain [0..5]       - Set PGA (0=6.144V, 2=2.048V, 5=0.256V)");
  Serial.println("  rate [0..7]       - Set data rate");
  Serial.println("  mode [single|cont] - Set operating mode");
  Serial.println();
  Serial.println("Driver Debugging:");
  Serial.println("  drv               - Show driver state and health");
  Serial.println("  probe             - Probe device (no health tracking)");
  Serial.println("  recover           - Manual recovery attempt");
  Serial.println("  verbose [0|1]     - Enable/disable verbose output");
  Serial.println("  stress [N]        - Run N conversion cycles");
  Serial.println("  config            - Dump config register");
  Serial.println("  scan              - Scan I2C bus");
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
  uint16_t config = 0;
  if (!readConfigFromDevice(config)) {
    return;
  }
  ADS1115::Mux mux =
      static_cast<ADS1115::Mux>((config & ADS1115::cmd::MASK_MUX) >> ADS1115::cmd::BIT_MUX);
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
  uint16_t config = 0;
  if (!readConfigFromDevice(config)) {
    return;
  }
  ADS1115::Gain gain =
      static_cast<ADS1115::Gain>((config & ADS1115::cmd::MASK_PGA) >> ADS1115::cmd::BIT_PGA);
  Serial.printf("  Gain: %u (%s)\n", static_cast<unsigned>(gain), gainToStr(gain));
}

void printCurrentRate() {
  uint16_t config = 0;
  if (!readConfigFromDevice(config)) {
    return;
  }
  ADS1115::DataRate rate =
      static_cast<ADS1115::DataRate>((config & ADS1115::cmd::MASK_DR) >> ADS1115::cmd::BIT_DR);
  Serial.printf("  Rate: %u (%s)\n", static_cast<unsigned>(rate), rateToStr(rate));
}

void printCurrentMode() {
  uint16_t config = 0;
  if (!readConfigFromDevice(config)) {
    return;
  }
  ADS1115::Mode mode =
      static_cast<ADS1115::Mode>((config & ADS1115::cmd::MASK_MODE) >> ADS1115::cmd::BIT_MODE);
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
}

bool readConfigFromDevice(uint16_t& config) {
  ADS1115::Status st = device.readConfig(config);
  if (!st.ok()) {
    printStatus(st);
    return false;
  }
  return true;
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
  } else if (cmd == "scan") {
    i2c::scan();
  } else if (cmd == "probe") {
    LOGI("Probing device (no health tracking)...");
    auto st = device.probe();
    printStatus(st);
  } else if (cmd == "drv") {
    printDriverHealth();
  } else if (cmd == "recover") {
    LOGI("Attempting recovery...");
    auto st = device.recover();
    printStatus(st);
    printDriverHealth();
  } else if (cmd == "verbose") {
    LOGI("Verbose mode: %s", verboseMode ? "ON" : "OFF");
  } else if (cmd.startsWith("verbose ")) {
    int val = cmd.substring(8).toInt();
    verboseMode = (val != 0);
    LOGI("Verbose mode: %s", verboseMode ? "ON" : "OFF");
  } else if (cmd == "start") {
    auto st = device.startConversion();
    printStatus(st);
  } else if (cmd == "poll") {
    bool ready = device.conversionReady();
    LOGI("Conversion ready: %s", ready ? "YES" : "NO");
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
    int count = cmd.substring(5).toInt();
    if (count <= 0) {
      LOGW("Invalid count");
      return;
    }
    for (int i = 0; i < count; ++i) {
      int16_t raw = 0;
      auto st = device.readBlocking(raw);
      if (!st.ok()) {
        printStatus(st);
        break;
      }
      Serial.printf("  %d: %d (%.6f V)\n", i + 1, raw, device.rawToVoltage(raw));
    }
  } else if (cmd == "ch") {
    printCurrentMux();
  } else if (cmd.startsWith("ch ")) {
    int channel = cmd.substring(3).toInt();
    if (channel < 0 || channel > 3) {
      LOGW("Invalid channel");
      return;
    }
    auto st = device.setMux(channelToMux(channel));
    printStatus(st);
  } else if (cmd == "diff") {
    printCurrentMux();
  } else if (cmd.startsWith("diff ")) {
    int idx = cmd.substring(5).toInt();
    if (idx < 0 || idx > 3) {
      LOGW("Invalid differential index");
      return;
    }
    auto st = device.setMux(diffToMux(idx));
    printStatus(st);
  } else if (cmd == "gain") {
    printCurrentGain();
  } else if (cmd.startsWith("gain ")) {
    int gain = cmd.substring(5).toInt();
    if (gain < 0 || gain > 5) {
      LOGW("Invalid gain");
      return;
    }
    auto st = device.setGain(static_cast<ADS1115::Gain>(gain));
    printStatus(st);
  } else if (cmd == "rate") {
    printCurrentRate();
  } else if (cmd.startsWith("rate ")) {
    int rate = cmd.substring(5).toInt();
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
  } else if (cmd.startsWith("stress")) {
    int count = 10;
    if (cmd.length() > 6) {
      count = cmd.substring(7).toInt();
    }
    if (count <= 0) {
      LOGW("Invalid count");
      return;
    }
    int ok = 0;
    int fail = 0;
    for (int i = 0; i < count; ++i) {
      int16_t raw = 0;
      auto st = device.readBlocking(raw);
      if (st.ok()) {
        ok++;
        LOGV(verboseMode, "  %d: %d (%.6f V)", i + 1, raw, device.rawToVoltage(raw));
      } else {
        fail++;
        if (verboseMode) {
          printStatus(st);
        }
      }
    }
    Serial.printf("  Stress results: %d ok, %d failed\n", ok, fail);
  } else if (cmd == "config") {
    printConfig();
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

  i2c::scan();

  ADS1115::Config cfg;
  cfg.i2cWrite = transport::wireWrite;
  cfg.i2cWriteRead = transport::wireWriteRead;
  cfg.i2cAddress = 0x48;
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
  Serial.print("> ");
}

void loop() {
  device.tick(millis());

  static String inputBuffer;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
        Serial.print("> ");
      }
    } else {
      inputBuffer += c;
    }
  }
}
