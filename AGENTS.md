# AGENTS.md - ADS1115 Production Embedded Guidelines

## Role and Target
You are a professional embedded software engineer building a production-grade ADS1115 16-bit ADC library.

- Target: ESP32-S2 / ESP32-S3, Arduino framework, PlatformIO.
- Goals: deterministic behavior, long-term stability, clean API contracts, portability, no surprises in the field.
- These rules are binding.

---

## Repository Model (Single Library)

```
include/ADS1115/         - Public API headers only (Doxygen)
  CommandTable.h         - Register addresses and bit masks
  Status.h
  Config.h
  ADS1115.h
  Version.h              - Auto-generated (do not edit)
src/                     - Implementation (.cpp)
examples/
  00_*/
  01_*/
  common/                - Example-only helpers (Log.h, BoardConfig.h, I2cTransport.h,
                           I2cScanner.h, CommandHandler.h)
platformio.ini
library.json
README.md
CHANGELOG.md
AGENTS.md
```

Rules:
- `examples/common/` is NOT part of the library. It simulates project glue and keeps examples self-contained.
- No board-specific pins/bus in library code; only in `Config`.
- Public headers only in `include/ADS1115/`.
- Examples demonstrate usage and may use `examples/common/BoardConfig.h`.
- Keep the layout boring and predictable.

---

## Core Engineering Rules (Mandatory)

- Deterministic: no unbounded loops/waits; all timeouts via deadlines, never `delay()` in library code.
- Non-blocking lifecycle: `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`.
- Any I/O that can exceed ~1-2 ms must be split into state machine steps driven by `tick()`.
- No heap allocation in steady state (no `String`, `std::vector`, `new` in normal ops).
- No logging in library code; examples may log.
- No macros for constants; use `static constexpr`. Macros only for conditional compile or logging helpers.

---

## I2C Manager + Transport (Required)

- The library MUST NOT own I2C. It never touches `Wire` directly.
- `Config` MUST accept a transport adapter (function pointers or abstract interface).
- Transport errors MUST map to `Status` (no leaking `Wire`, `esp_err_t`, etc.).
- The library MUST NOT configure bus timeouts or pins.

---

## Status / Error Handling (Mandatory)

All fallible APIs return `Status`:

```cpp
struct Status {
  Err code;
  int32_t detail;
  const char* msg;  // static string only
};
```

- Silent failure is unacceptable.
- No exceptions.

---

## ADS1115 Driver Requirements

- I2C address configurable: 0x48 (ADDR→GND), 0x49 (ADDR→VDD), 0x4A (ADDR→SDA), 0x4B (ADDR→SCL).
- Check device presence in `begin()` by reading config register.
- Support input multiplexer configurations:
  - 4 single-ended inputs (AIN0-AIN3 vs GND)
  - 3 differential pairs (AIN0-AIN1, AIN0-AIN3, AIN1-AIN3, AIN2-AIN3)
- Configurable PGA (gain): ±6.144V, ±4.096V, ±2.048V, ±1.024V, ±0.512V, ±0.256V
- Configurable data rate: 8, 16, 32, 64, 128, 250, 475, 860 SPS
- Support operating modes:
  - **Single-shot mode**: One conversion on demand, then power down
  - **Continuous mode**: Continuous conversions at configured data rate
- Comparator support (optional): window/traditional mode, latching, active high/low, queue depth
- Conversion ready detection via:
  - Polling OS bit in config register
  - ALERT/RDY pin (comparator mode with thresholds set appropriately)
- Proper 16-bit signed result handling (two's complement)
- Voltage calculation from raw ADC value and gain setting

---

## Driver Architecture: Managed Synchronous Driver

The driver follows a **managed synchronous** model with health tracking:

- All public I2C operations are **blocking** (no complex async - ADS1115 has no EEPROM/NVM writes).
- `tick()` may be used for single-shot conversion wait or continuous mode polling.
- Health is tracked via **tracked transport wrappers** — public API never calls `_updateHealth()` directly.
- Recovery is **manual** via `recover()` - the application controls retry strategy.

### DriverState (4 states only)

```cpp
enum class DriverState : uint8_t {
  UNINIT,    // begin() not called or end() called
  READY,     // Operational, consecutiveFailures == 0
  DEGRADED,  // 1 <= consecutiveFailures < offlineThreshold
  OFFLINE    // consecutiveFailures >= offlineThreshold
};
```

State transitions:
- `begin()` success → READY
- Any I2C failure in READY → DEGRADED
- Success in DEGRADED/OFFLINE → READY
- Failures reach `offlineThreshold` → OFFLINE
- `end()` → UNINIT

### Transport Wrapper Architecture

All I2C goes through layered wrappers:

```
Public API (readAdc, startConversion, etc.)
    ↓
Register helpers (readRegs, writeRegs)
    ↓
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    ↓  ← _updateHealth() called here ONLY
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    ↓
Transport callbacks (Config::i2cWrite, i2cWriteRead)
```

**Rules:**
- Public API methods NEVER call `_updateHealth()` directly
- `readRegs()`/`writeRegs()` use TRACKED wrappers → health updated automatically
- `probe()` uses RAW wrappers → no health tracking (diagnostic only)
- `recover()` tracks probe failures (driver is initialized, so failures count)

### Health Tracking Rules

- `_updateHealth()` called ONLY inside tracked transport wrappers.
- State transitions guarded by `_initialized` (no DEGRADED/OFFLINE before `begin()` succeeds).
- NOT called for config/param validation errors (INVALID_CONFIG, INVALID_PARAM).
- NOT called for precondition errors (NOT_INITIALIZED).
- `probe()` uses raw I2C and does NOT update health (diagnostic only).

### Health Tracking Fields

- `_lastOkMs` - timestamp of last successful I2C operation
- `_lastErrorMs` - timestamp of last failed I2C operation
- `_lastError` - most recent error Status
- `_consecutiveFailures` - failures since last success (resets on success)
- `_totalFailures` / `_totalSuccess` - lifetime counters (wrap at max)

---

## Versioning and Releases

Single source of truth: `library.json`. `Version.h` is auto-generated and must never be edited.

SemVer:
- MAJOR: breaking API/Config/enum changes.
- MINOR: new backward-compatible features or error codes (append only).
- PATCH: bug fixes, refactors, docs.

Release steps:
1. Update `library.json`.
2. Update `CHANGELOG.md` (Added/Changed/Fixed/Removed).
3. Update `README.md` if API or examples changed.
4. Commit and tag: `Release vX.Y.Z`.

---

## Naming Conventions

- Member variables: `_camelCase`
- Methods/Functions: `camelCase`
- Constants: `CAPS_CASE`
- Enum values: `CAPS_CASE` or short forms (e.g., `AIN0_GND`)
- Locals/params: `camelCase`
- Config fields: `camelCase`
