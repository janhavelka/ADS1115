# Codex Prompt: Production-Grade ADS1115 I2C Driver Library (ESP32 Arduino / PlatformIO)

You are an expert embedded engineer. Implement a **production-grade, reusable ADS1115 16-bit ADC driver library** for **ESP32-S2 / ESP32-S3** using **Arduino framework** under **PlatformIO**.

---

## Baseline Contracts (DO NOT RESTATE)

- **`AGENTS.md`** defines: repository layout, non-blocking architecture, injected I2C transport rules, deterministic tick behavior, managed synchronous driver pattern, transport wrapper architecture, health tracking, and DriverState model.
- **`ADS1115_Register_Reference.md`** is the authoritative register map (if provided).
- **Do not duplicate those requirements.** Implement them and only document what is *not already covered*.

---

## Deliverables

Generate a complete library repository:
- All headers in `include/ADS1115/` (Status.h, Config.h, ADS1115.h, CommandTable.h, Version.h)
- Implementation in `src/ADS1115.cpp`
- Examples in `examples/01_basic_bringup_cli/`
- Example helpers in `examples/common/` (Log.h, BoardConfig.h, I2cTransport.h)
- README.md, CHANGELOG.md, library.json, platformio.ini
- Version generation script in `scripts/generate_version.py`

**This is a library, not an application.**

---

## Namespace

All types in `namespace ADS1115 { }`.

---

## Status.h — Error Codes

```cpp
enum class Err : uint8_t {
  OK = 0,                    ///< Operation successful
  NOT_INITIALIZED,           ///< begin() not called
  INVALID_CONFIG,            ///< Invalid configuration parameter
  I2C_ERROR,                 ///< I2C communication failure
  TIMEOUT,                   ///< Operation timed out
  INVALID_PARAM,             ///< Invalid parameter value
  DEVICE_NOT_FOUND,          ///< ADS1115 not responding on I2C bus
  CONVERSION_NOT_READY,      ///< Conversion not yet complete
  BUSY,                      ///< Device is busy with conversion
  IN_PROGRESS                ///< Operation scheduled; call tick() to complete
};

struct Status {
  Err code = Err::OK;
  int32_t detail = 0;
  const char* msg = "";
  
  constexpr bool ok() const { return code == Err::OK; }
  static constexpr Status Ok() { return Status{Err::OK, 0, "OK"}; }
  static constexpr Status Error(Err err, const char* message, int32_t detailCode = 0);
};
```

---

## Config.h — Configuration

```cpp
/// I2C write callback signature
using I2cWriteFn = Status (*)(uint8_t addr, const uint8_t* data, size_t len,
                              uint32_t timeoutMs, void* user);

/// I2C write-read callback signature  
using I2cWriteReadFn = Status (*)(uint8_t addr, const uint8_t* tx, size_t txLen,
                                  uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                  void* user);

/// Input multiplexer configuration
enum class Mux : uint8_t {
  AIN0_AIN1 = 0,  ///< Differential: AIN0 - AIN1 (default)
  AIN0_AIN3 = 1,  ///< Differential: AIN0 - AIN3
  AIN1_AIN3 = 2,  ///< Differential: AIN1 - AIN3
  AIN2_AIN3 = 3,  ///< Differential: AIN2 - AIN3
  AIN0_GND  = 4,  ///< Single-ended: AIN0
  AIN1_GND  = 5,  ///< Single-ended: AIN1
  AIN2_GND  = 6,  ///< Single-ended: AIN2
  AIN3_GND  = 7   ///< Single-ended: AIN3
};

/// Programmable Gain Amplifier (full-scale range)
enum class Gain : uint8_t {
  FSR_6_144V = 0,  ///< ±6.144V (LSB = 187.5µV)
  FSR_4_096V = 1,  ///< ±4.096V (LSB = 125µV)
  FSR_2_048V = 2,  ///< ±2.048V (LSB = 62.5µV) - default
  FSR_1_024V = 3,  ///< ±1.024V (LSB = 31.25µV)
  FSR_0_512V = 4,  ///< ±0.512V (LSB = 15.625µV)
  FSR_0_256V = 5   ///< ±0.256V (LSB = 7.8125µV)
};

/// Data rate (samples per second)
enum class DataRate : uint8_t {
  SPS_8   = 0,   ///<   8 SPS
  SPS_16  = 1,   ///<  16 SPS
  SPS_32  = 2,   ///<  32 SPS
  SPS_64  = 3,   ///<  64 SPS
  SPS_128 = 4,   ///< 128 SPS (default)
  SPS_250 = 5,   ///< 250 SPS
  SPS_475 = 6,   ///< 475 SPS
  SPS_860 = 7    ///< 860 SPS
};

/// Operating mode
enum class Mode : uint8_t {
  CONTINUOUS  = 0,  ///< Continuous conversion mode
  SINGLE_SHOT = 1   ///< Single-shot / power-down mode (default)
};

/// Comparator mode
enum class ComparatorMode : uint8_t {
  TRADITIONAL = 0,  ///< Traditional comparator (default)
  WINDOW      = 1   ///< Window comparator
};

/// Comparator polarity
enum class ComparatorPolarity : uint8_t {
  ACTIVE_LOW  = 0,  ///< ALERT/RDY active low (default)
  ACTIVE_HIGH = 1   ///< ALERT/RDY active high
};

/// Comparator latch
enum class ComparatorLatch : uint8_t {
  NON_LATCHING = 0,  ///< Non-latching (default)
  LATCHING     = 1   ///< Latching
};

/// Comparator queue (assertions before ALERT)
enum class ComparatorQueue : uint8_t {
  ASSERT_1    = 0,  ///< Assert after 1 conversion
  ASSERT_2    = 1,  ///< Assert after 2 conversions
  ASSERT_4    = 2,  ///< Assert after 4 conversions
  DISABLE     = 3   ///< Disable comparator (default), ALERT/RDY high-Z
};

struct Config {
  // === I2C Transport (required) ===
  I2cWriteFn i2cWrite = nullptr;
  I2cWriteReadFn i2cWriteRead = nullptr;
  void* i2cUser = nullptr;
  
  // === Device Settings ===
  uint8_t i2cAddress = 0x48;       ///< 0x48-0x4B based on ADDR pin
  uint32_t i2cTimeoutMs = 50;      ///< I2C transaction timeout
  
  // === Conversion Settings ===
  Mux mux = Mux::AIN0_GND;                  ///< Input multiplexer
  Gain gain = Gain::FSR_2_048V;             ///< PGA gain
  DataRate dataRate = DataRate::SPS_128;    ///< Data rate
  Mode mode = Mode::SINGLE_SHOT;            ///< Operating mode
  
  // === Comparator Settings (optional) ===
  ComparatorMode compMode = ComparatorMode::TRADITIONAL;
  ComparatorPolarity compPolarity = ComparatorPolarity::ACTIVE_LOW;
  ComparatorLatch compLatch = ComparatorLatch::NON_LATCHING;
  ComparatorQueue compQueue = ComparatorQueue::DISABLE;
  int16_t compThresholdHigh = 0x7FFF;  ///< High threshold (default: max)
  int16_t compThresholdLow = 0x8000;   ///< Low threshold (default: min)
  
  // === Health Tracking ===
  uint8_t offlineThreshold = 5;    ///< Consecutive failures before OFFLINE
};
```

---

## CommandTable.h — Register Definitions

```cpp
namespace cmd {
  // Register addresses (pointer register values)
  static constexpr uint8_t REG_CONVERSION = 0x00;  ///< Conversion result (16-bit, read-only)
  static constexpr uint8_t REG_CONFIG     = 0x01;  ///< Configuration register (16-bit, R/W)
  static constexpr uint8_t REG_LO_THRESH  = 0x02;  ///< Low threshold (16-bit, R/W)
  static constexpr uint8_t REG_HI_THRESH  = 0x03;  ///< High threshold (16-bit, R/W)
  
  // Config register bit positions (16-bit register)
  static constexpr uint8_t BIT_OS        = 15;  ///< Operational status / single-shot start
  static constexpr uint8_t BIT_MUX       = 12;  ///< Mux (3 bits: 14:12)
  static constexpr uint8_t BIT_PGA       = 9;   ///< PGA (3 bits: 11:9)
  static constexpr uint8_t BIT_MODE      = 8;   ///< Mode (1 bit)
  static constexpr uint8_t BIT_DR        = 5;   ///< Data rate (3 bits: 7:5)
  static constexpr uint8_t BIT_COMP_MODE = 4;   ///< Comparator mode (1 bit)
  static constexpr uint8_t BIT_COMP_POL  = 3;   ///< Comparator polarity (1 bit)
  static constexpr uint8_t BIT_COMP_LAT  = 2;   ///< Comparator latch (1 bit)
  static constexpr uint8_t BIT_COMP_QUE  = 0;   ///< Comparator queue (2 bits: 1:0)
  
  // Config register masks
  static constexpr uint16_t MASK_OS        = 0x8000;
  static constexpr uint16_t MASK_MUX       = 0x7000;
  static constexpr uint16_t MASK_PGA       = 0x0E00;
  static constexpr uint16_t MASK_MODE      = 0x0100;
  static constexpr uint16_t MASK_DR        = 0x00E0;
  static constexpr uint16_t MASK_COMP_MODE = 0x0010;
  static constexpr uint16_t MASK_COMP_POL  = 0x0008;
  static constexpr uint16_t MASK_COMP_LAT  = 0x0004;
  static constexpr uint16_t MASK_COMP_QUE  = 0x0003;
  
  // OS bit meanings
  static constexpr uint16_t OS_IDLE        = 0x8000;  ///< Read: no conversion in progress
  static constexpr uint16_t OS_START       = 0x8000;  ///< Write: start single conversion
  
  // Default config register value
  static constexpr uint16_t CONFIG_DEFAULT = 0x8583;
}
```

---

## ADS1115.h — Main Driver Class

### DriverState (same pattern)

```cpp
enum class DriverState : uint8_t {
  UNINIT,    ///< begin() not called or end() called
  READY,     ///< Operational, consecutiveFailures == 0
  DEGRADED,  ///< 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    ///< consecutiveFailures >= offlineThreshold
};
```

### Public API

```cpp
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
  DriverState state() const;
  bool isOnline() const;
  
  // === Health Tracking ===
  uint32_t lastOkMs() const;
  uint32_t lastErrorMs() const;
  Status lastError() const;
  uint8_t consecutiveFailures() const;
  uint32_t totalFailures() const;
  uint32_t totalSuccess() const;
  
  // === Conversion API ===
  
  /// Start a single-shot conversion (non-blocking)
  /// Returns IN_PROGRESS if started, BUSY if already converting
  Status startConversion();
  
  /// Start conversion on specific channel (changes mux, then starts)
  Status startConversion(Mux mux);
  
  /// Check if conversion is complete
  /// Returns true if result ready to read
  bool conversionReady();
  
  /// Read conversion result as raw 16-bit signed value
  /// Returns CONVERSION_NOT_READY if not complete
  Status readRaw(int16_t& out);
  
  /// Read conversion result as voltage (using current gain setting)
  Status readVoltage(float& volts);
  
  /// Blocking read: start conversion, wait, return result
  /// Only use if blocking is acceptable; respects timeout
  Status readBlocking(int16_t& out, uint32_t timeoutMs = 200);
  Status readBlockingVoltage(float& volts, uint32_t timeoutMs = 200);
  
  // === Configuration ===
  
  /// Set input multiplexer
  Status setMux(Mux mux);
  Mux getMux() const;
  
  /// Set PGA gain
  Status setGain(Gain gain);
  Gain getGain() const;
  
  /// Set data rate
  Status setDataRate(DataRate rate);
  DataRate getDataRate() const;
  
  /// Set operating mode
  Status setMode(Mode mode);
  Mode getMode() const;
  
  /// Read current config register
  Status readConfig(uint16_t& config);
  
  /// Write config register
  Status writeConfig(uint16_t config);
  
  // === Comparator ===
  
  /// Set comparator thresholds
  Status setThresholds(int16_t low, int16_t high);
  
  /// Get comparator thresholds
  Status getThresholds(int16_t& low, int16_t& high);
  
  /// Configure comparator for conversion-ready mode (ALERT/RDY pin)
  /// Sets thresholds to trigger after every conversion
  Status enableConversionReadyPin();
  
  /// Disable comparator (ALERT/RDY high-Z)
  Status disableComparator();
  
  // === Utility ===
  
  /// Convert raw ADC value to voltage based on gain setting
  float rawToVoltage(int16_t raw) const;
  
  /// Get LSB size in volts for current gain
  float getLsbVoltage() const;
  
  /// Get conversion time in milliseconds for current data rate
  uint32_t getConversionTimeMs() const;

private:
  // === Transport Wrappers ===
  Status _i2cWriteReadRaw(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen);
  Status _i2cWriteRaw(const uint8_t* buf, size_t len);
  Status _i2cWriteReadTracked(const uint8_t* txBuf, size_t txLen, uint8_t* rxBuf, size_t rxLen);
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
  uint32_t _conversionStartMs = 0;
  int16_t _lastRawValue = 0;
};
```

---

## Implementation Requirements

### Transport Wrapper Architecture

Follow the established pattern exactly:

```
Public API (readRaw, startConversion, etc.)
    ↓
Register helpers (readRegister16, writeRegister16)
    ↓
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    ↓  ← _updateHealth() called here ONLY
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    ↓
Transport callbacks (Config::i2cWrite, i2cWriteRead)
```

### begin() Flow

1. Store config, reset state, reset health counters
2. Set `_initialized = false`, `_driverState = UNINIT`
3. Validate config (callbacks not null, timeout > 0, address in range)
4. Clamp `offlineThreshold` to minimum 1
5. `probe()` — verify device responds (raw path, no health tracking)
6. `_applyConfig()` — write config register with all settings
7. Set `_initialized = true`, `_driverState = READY`

### Voltage Calculation

```cpp
float rawToVoltage(int16_t raw) const {
  // LSB size depends on PGA setting
  static constexpr float lsbTable[] = {
    187.5e-6f,   // FSR_6_144V
    125.0e-6f,   // FSR_4_096V
    62.5e-6f,    // FSR_2_048V
    31.25e-6f,   // FSR_1_024V
    15.625e-6f,  // FSR_0_512V
    7.8125e-6f   // FSR_0_256V
  };
  return raw * lsbTable[static_cast<uint8_t>(_config.gain)];
}
```

### Conversion Timing

```cpp
uint32_t getConversionTimeMs() const {
  // Conversion time = 1/SPS + margin
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
  return timeTable[static_cast<uint8_t>(_config.dataRate)];
}
```

### tick() Behavior

In single-shot mode with pending conversion:
1. Check if conversion time elapsed
2. Read OS bit to confirm conversion complete
3. Set ready flag

In continuous mode:
- Optionally poll for new data if requested

### Conversion Ready Pin

To use ALERT/RDY as conversion-ready signal:
```cpp
Status enableConversionReadyPin() {
  // Set Lo_thresh = 0x0000, Hi_thresh = 0x8000
  // This triggers ALERT after every conversion
  Status st = writeRegister16(REG_LO_THRESH, 0x0000);
  if (!st.ok()) return st;
  return writeRegister16(REG_HI_THRESH, 0x8000);
}
```

---

## Example: 01_basic_bringup_cli

Interactive CLI demonstrating all features:

```
Commands:
  help              - Show this help
  read              - Read single conversion (blocking)
  read N            - Read N conversions
  start             - Start single-shot conversion
  poll              - Check if conversion ready
  raw               - Read raw value
  voltage           - Read as voltage
  
Channel/Gain:
  ch 0|1|2|3        - Set single-ended channel (AINx vs GND)
  diff 0|1|2|3      - Set differential pair
  gain 0..5         - Set PGA (0=6.144V, 2=2.048V default, 5=0.256V)
  rate 0..7         - Set data rate (4=128SPS default)
  mode single|cont  - Set operating mode
  
Driver Debugging:
  drv               - Show driver state and health
  probe             - Probe device (no health tracking)
  recover           - Manual recovery attempt
  verbose 0|1       - Enable/disable verbose output
  stress [N]        - Run N conversion cycles
  config            - Dump config register
```

---

## examples/common/ Files

### Log.h
```cpp
#define LOGI(fmt, ...) Serial.printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOGW(fmt, ...) Serial.printf("[W] " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) Serial.printf("[E] " fmt "\n", ##__VA_ARGS__)
```

### BoardConfig.h
```cpp
namespace board {
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
    static constexpr int I2C_SDA = 8;
    static constexpr int I2C_SCL = 9;
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    static constexpr int I2C_SDA = 8;
    static constexpr int I2C_SCL = 9;
  #endif
  
  bool initI2c();
}
```

### I2cTransport.h
```cpp
namespace transport {
  ADS1115::Status wireWrite(uint8_t addr, const uint8_t* data, size_t len,
                            uint32_t timeoutMs, void* user);
  ADS1115::Status wireWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                                uint8_t* rx, size_t rxLen, uint32_t timeoutMs,
                                void* user);
}
```

---

## platformio.ini

```ini
[platformio]
default_envs = ex_bringup_s3

[env]
framework = arduino
monitor_speed = 115200
lib_deps = 
build_flags = 
  -std=c++17
  -Wall
  -Wextra

[env:ex_bringup_s3]
platform = espressif32
board = esp32-s3-devkitc-1
build_src_filter = +<examples/01_basic_bringup_cli/>
extra_scripts = pre:scripts/generate_version.py

[env:ex_bringup_s2]
platform = espressif32
board = esp32-s2-saola-1
build_src_filter = +<examples/01_basic_bringup_cli/>
extra_scripts = pre:scripts/generate_version.py
```

---

## library.json

```json
{
  "name": "ADS1115",
  "version": "1.0.0",
  "description": "Production-grade ADS1115 16-bit ADC driver",
  "keywords": ["ads1115", "adc", "analog", "i2c", "16-bit"],
  "repository": {
    "type": "git",
    "url": "https://github.com/user/ADS1115.git"
  },
  "authors": [
    {
      "name": "Your Name"
    }
  ],
  "license": "MIT",
  "frameworks": ["arduino"],
  "platforms": ["espressif32"]
}
```

---

## Implementation Constraints (DO NOT VIOLATE)

- No heap allocation in steady-state
- No Wire dependency in library code
- No logging in library code (examples may log)
- Bounded deterministic work per tick()
- Injected transport only
- Never `delay()` in library code
- All public I2C operations are blocking
- Health tracking only via tracked transport wrappers
- State transitions guarded by `_initialized`

---

## Final Output

Generate all repository files with correct, compile-ready, production-quality code.
Ensure examples build for ESP32-S3.

**Now implement the full library.**
