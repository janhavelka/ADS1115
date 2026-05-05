# ADS1115 Driver Library

Production-grade ADS1115 16-bit ADC I2C driver for ESP32-S2 / ESP32-S3
(Arduino framework, PlatformIO).

## Features

- Injected I2C transport (no Wire dependency in library code)
- Health monitoring with READY / DEGRADED / OFFLINE states
- Single-shot and continuous conversion modes
- Configurable mux, gain, data rate, and comparator settings
- Conversion readiness through bounded OS-bit polling or ALERT/RDY pin mode
- Raw and voltage conversion helpers

## Installation

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
  ADS1115
```

### Manual

Copy `include/ADS1115/` and `src/` into your project.

## Quick Start

```cpp
#include <Wire.h>
#include "ADS1115/ADS1115.h"

// Transport callbacks
ADS1115::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                         uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;  // Manager-owned in shared-bus setups.
  wire->beginTransmission(addr);
  wire->write(data, len);
  switch (wire->endTransmission(true)) {
    case 0: return ADS1115::Status::Ok();
    case 2: return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_ADDR, "Address NACK");
    case 3: return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_DATA, "Data NACK");
    case 5: return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "I2C timeout");
    case 4: return ADS1115::Status::Error(ADS1115::Err::I2C_BUS, "I2C bus error");
    default: return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Write failed");
  }
}

ADS1115::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                             uint8_t* rx, size_t rxLen,
                             uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;  // Manager-owned in shared-bus setups.
  wire->beginTransmission(addr);
  wire->write(tx, txLen);
  uint8_t result = wire->endTransmission(false);
  if (result != 0) {
    return ADS1115::Status::Error(
      result == 2 ? ADS1115::Err::I2C_NACK_ADDR :
      result == 3 ? ADS1115::Err::I2C_NACK_DATA :
      result == 5 ? ADS1115::Err::I2C_TIMEOUT :
      result == 4 ? ADS1115::Err::I2C_BUS :
                    ADS1115::Err::I2C_ERROR,
      "Write phase failed");
  }
  if (wire->requestFrom(addr, rxLen) != rxLen) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Read failed");
  }
  for (size_t i = 0; i < rxLen; ++i) {
    rx[i] = wire->read();
  }
  return ADS1115::Status::Ok();
}

ADS1115::ADS1115 device;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  ADS1115::Config cfg;
  cfg.i2cWrite = i2cWrite;
  cfg.i2cWriteRead = i2cWriteRead;
  cfg.i2cUser = &Wire;
  cfg.i2cAddress = 0x48;

  auto status = device.begin(cfg);
  if (!status.ok()) {
    Serial.printf("Init failed: %s\n", status.msg);
    return;
  }

  Serial.println("ADS1115 initialized!");
}

void loop() {
  device.tick(millis());

  int16_t raw = 0;
  ADS1115::Status st = device.readBlocking(raw);
  if (st.ok()) {
    Serial.printf("Raw=%d Voltage=%.6f V\n", raw, device.rawToVoltage(raw));
  }
}
```

## API Overview

### Lifecycle

| Method | Description |
|--------|-------------|
| `begin(config)` | Initialize with injected transport and verify the device is reachable |
| `tick(nowMs)` | Process bounded conversion polling work |
| `end()` | Best-effort return the ADC to single-shot idle and clear cached conversion state |
| `isInitialized()` | True after successful `begin()` until `end()` |
| `getConfig()` | Return the driver's cached configuration snapshot |

### Diagnostics And Raw Access

| Method | Description |
|--------|-------------|
| `probe()` | Check device presence without updating health counters |
| `recover()` | Re-validate comms, clear conversion state, and re-apply cached config |
| `getSettings(snap)` | Populate a `SettingsSnapshot` with cached config and runtime state (no I2C) |
| `readRegister16(reg, value)` | Read a raw 16-bit register using tracked I2C |
| `writeRegister16(reg, value)` | Write a raw 16-bit register using tracked I2C |
| `readRegister(reg, value)` | Compatibility alias for `readRegister16()` |
| `writeRegister(reg, value)` | Compatibility alias for `writeRegister16()` |

Raw register helpers accept only ADS1115 registers `0x00..0x03`.

### Conversion

| Method | Description |
|--------|-------------|
| `startConversion()` | Start a single-shot conversion with the current mux; returns `Err::IN_PROGRESS` when scheduled |
| `startConversion(mux)` | Start a single-shot conversion after atomically applying a temporary mux |
| `readConversionReady(ready)` | Report readiness with explicit I2C error status |
| `conversionReady()` | Convenience readiness check; returns `false` on transport failure |
| `readRaw(out)` | Read signed two's-complement conversion data |
| `readVoltage(volts)` | Read conversion data and scale it using the active gain |
| `readBlocking(out, timeoutMs)` | Start/join a single-shot conversion and wait with a finite deadline |
| `readBlockingVoltage(volts, timeoutMs)` | Blocking read with voltage scaling |

In single-shot mode, readiness is determined by ALERT/RDY when conversion-ready
pin mode is configured, otherwise by polling the OS bit after the configured
conversion time. In continuous mode the driver tracks the configured data-rate
interval between reads; ALERT/RDY can be used when wired and configured.

### Configuration

| Method | Description |
|--------|-------------|
| `setMux(mux)` | Select one of four single-ended inputs or four differential pairs |
| `setGain(gain)` | Select PGA full-scale range from +/-6.144 V to +/-0.256 V |
| `setDataRate(rate)` | Select 8, 16, 32, 64, 128, 250, 475, or 860 SPS |
| `setMode(mode)` | Select single-shot or continuous mode |
| `readConfig(config)` | Read the CONFIG register |
| `writeConfig(config)` | Write a validated CONFIG register value and sync the cache |

Typed setters validate enum values and update the cached configuration only
after the required I2C writes succeed. If a multi-register update is partially
written and then fails, `recover()` reapplies the cached configuration.

### Comparator And ALERT/RDY

| Method | Description |
|--------|-------------|
| `setThresholds(low, high)` | Write signed comparator threshold registers |
| `getThresholds(low, high)` | Read threshold registers and sync the cache |
| `setComparatorMode(mode)` | Select traditional or window comparator mode |
| `setComparatorPolarity(polarity)` | Select active-low or active-high ALERT/RDY polarity |
| `setComparatorLatch(latch)` | Select latching or non-latching comparator behavior |
| `setComparatorQueue(queue)` | Select assert-after count or disable comparator |
| `enableConversionReadyPin()` | Program ADS1115 conversion-ready threshold mode |
| `disableComparator()` | Disable comparator output |

### Configuration Constraints

| Field | Valid Values |
|-------|--------------|
| `i2cWrite`, `i2cWriteRead` | Required transport callbacks |
| `i2cAddress` | `0x48`, `0x49`, `0x4A`, or `0x4B` |
| `i2cTimeoutMs` | Must be greater than zero |
| `alertRdyPin` | `-1` when unused; otherwise requires `gpioRead` |
| `offlineThreshold` | Zero is normalized to one |
| enum fields | Must be one of the documented enum values |

## Examples

- `examples/01_basic_bringup_cli/` - interactive CLI for ADS1115 features
- CLI register diagnostics: `reg <0..3>` and `wreg <0..3> <val>` allow raw register access for bring-up and service work. Raw writes bypass the typed config helpers; use `recover()` or `begin()` to restore cached settings after manual register edits.

### Example Helpers (`examples/common/`)

Not part of the library. These simulate project-level glue and keep examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Pin definitions and Wire init for supported boards |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Wire-based I2C transport adapter (`wireWrite`, `wireWriteRead`, `initWire`) |
| `I2cScanner.h` | I2C bus scanner with table output and bus recovery |
| `BusDiag.h` | Bus diagnostics wrapper (scan + probe) |
| `CliStyle.h` | Shared ANSI colors and CLI formatting helpers |
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## Behavioral Contracts

1. Threading model: single-threaded by default; not thread-safe.
2. Timing model: `tick()` is bounded; conversion polling and readiness checks stay transport-timeout-bounded.
3. Resource ownership: bus, pins, and timeout policy remain application-owned via `Config`.
4. Memory behavior: no heap allocation in steady-state library operation.
5. Error handling: all fallible APIs return `Status`; no exceptions and no silent failures.
6. Health behavior: `OFFLINE` is latched. Normal public I2C operations return `BUSY` with `Driver is offline; call recover()` without touching the bus until `recover()` succeeds.

## Validation

```bash
pio test -e native
python tools/check_cli_contract.py
python tools/check_core_timing_guard.py
pio run -e esp32s3dev
pio run -e esp32s2dev
```

## Documentation

- `CHANGELOG.md` - full release history
- `docs/IDF_PORT.md` - ESP-IDF portability guidance
- `include/ADS1115/CommandTable.h` - public register map, masks, and defaults
- `docs/ads1115.pdf` - TI datasheet copy used for driver verification
- `docs/TI_registry_reference/README.md` - TI reference-driver extraction notes

## License

MIT License. See `LICENSE`.
