# ADS1115 Driver Library

Production-grade ADS1115 16-bit ADC I2C driver for ESP32-S2 / ESP32-S3
(Arduino framework, PlatformIO).

## Features

- Injected I2C transport (no Wire dependency in library code)
- Health monitoring with READY / DEGRADED / OFFLINE states
- Single-shot and continuous conversion modes
- Poll-chunked single-shot and config-apply jobs with caller-owned instruction budgets
- Configurable mux, gain, data rate, and comparator settings
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

uint32_t arduinoNowMs(void*) {
  return millis();
}

void arduinoYield(void*) {
  yield();
}

ADS1115::ADS1115 device;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  ADS1115::Config cfg;
  cfg.i2cWrite = i2cWrite;
  cfg.i2cWriteRead = i2cWriteRead;
  cfg.i2cUser = &Wire;
  cfg.nowMs = arduinoNowMs;
  cfg.cooperativeYield = arduinoYield;
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
}
```

## API Overview

Public APIs are classified by intended use. "Steady-path candidate" means the
method can fit an application-owned scheduler when its I/O budget is acceptable;
it does not mean the full operation is always one I2C transfer.

### Steady-Path Candidates

| Method | Description |
|--------|-------------|
| `tick(nowMs)` | Process bounded conversion polling work |
| `startConversion([mux])` | Start a single-shot conversion using the split start/poll/read shape |
| `conversionReady()` | Check readiness using ALERT/RDY or config OS polling |
| `readRaw(out)` | Read the conversion register as signed `int16_t` |
| `readVoltage(volts)` | Read raw and convert to volts; raw integer paths are preferred for strict steady paths |
| `startSingleShot([mux])`, `pollSingleShot(...)` | Staged single-shot API; sequencing details belong to the companion poll-chunking work |
| `startApplyConfigJob()`, `pollApplyConfig(...)` | Staged cached-config apply API; sequencing details belong to the companion poll-chunking work |
| `cancelJob()`, `jobActive()`, `jobState()`, `lastJobStatus()`, `lastRawValue()` | Job state helpers without direct I2C |
| Cached getters and utility helpers | `getMux()`, `getGain()`, `getDataRate()`, `getMode()`, comparator getters, `rawToVoltage()`, `getLsbVoltage()`, `getConversionTimeMs()` |

### Lifecycle And Setup

| Method | Description |
|--------|-------------|
| `begin(config)` | Initialize with injected transport, verify presence, and apply cached config |
| `end()` | Best-effort return the ADC to single-shot idle and clear cached conversion state |
| `isInitialized()` | True after successful `begin()` until `end()` |
| `getConfig()` | Return the driver's cached configuration snapshot |
| `recover()` | Re-validate comms, clear conversion state, and re-apply cached config |
| `setMux()`, `setGain()`, `setDataRate()`, `setMode()` | Typed config setters for setup or controlled reconfiguration |
| Comparator setters | `setComparatorMode()`, `setComparatorPolarity()`, `setComparatorLatch()`, `setComparatorQueue()`, `enableConversionReadyPin()`, `disableComparator()` |
| Threshold helpers | `setThresholds(low, high)`, `getThresholds(low, high)` |

### Diagnostics And Raw Access

| Method | Description |
|--------|-------------|
| `probe()` | Check device presence without updating health counters |
| `getSettings(snap)` | Populate a `SettingsSnapshot` with cached config and runtime state (no I2C) |
| Health getters | `state()`, `isOnline()`, `lastOkMs()`, `lastErrorMs()`, `lastError()`, `consecutiveFailures()`, `totalFailures()`, `totalSuccess()` |
| `readConfig(config)` / `writeConfig(config)` | Direct config-register access for diagnostics and service tooling |
| `readRegister16(reg, value)` | Read a raw 16-bit register using tracked I2C |
| `writeRegister16(reg, value)` | Write a raw 16-bit register using tracked I2C |
| `readRegister(reg, value)` | Compatibility alias for `readRegister16()` |
| `writeRegister(reg, value)` | Compatibility alias for `writeRegister16()` |

### Blocking Convenience Only

| Method | Description |
|--------|-------------|
| `readBlocking(out, timeoutMs)` | Bounded blocking convenience wrapper around single-shot conversion |
| `readBlockingVoltage(volts, timeoutMs)` | Same bounded blocking wrapper plus voltage conversion |

`readBlocking()` and `readBlockingVoltage()` are intentionally retained for
bring-up, scripts, and low-rate convenience use. They are unsuitable for
TunnelMonitor-style steady paths because they can loop until the conversion
deadline or timeout, even though the wait is bounded by `timeoutMs` and the
configured transport timeout.

### Staged Polling Boundary

If staged polling APIs are present, use their returned status and instruction
counts as the scheduling contract. Exact per-poll sequencing and transaction
budget tests belong with the companion poll-chunking work, not with blocking
convenience APIs.

### Multi-Transfer And Cache Notes

`begin()`, `recover()`, typed config setters, comparator setters,
`setThresholds()`, `getThresholds()`, and raw register/config access may perform
multiple I2C transfers. Keep them on lifecycle/setup or diagnostic paths unless
the application explicitly schedules their budget with the poll-chunked job API.

Partial config-apply failures can leave the driver's cached config and the
hardware state uncertain if one write reached the ADC and a later transfer
failed. Inspect `SettingsSnapshot::hardwareConfigDirty`,
`SettingsSnapshot::hardwareConfigUncertain`, and
`SettingsSnapshot::lastConfigApplyError`, or the matching
`isHardwareConfigDirty()`, `isHardwareConfigUncertain()`, and
`lastConfigApplyError()` accessors. Treat a dirty or uncertain config as
requiring operator-visible diagnostics and a successful `recover()` or `begin()`
before trusting cached settings again.

## Examples

- `examples/01_basic_bringup_cli/` - interactive CLI for ADS1115 features
- CLI register diagnostics: `reg <addr>` and `wreg <addr> <val>` allow raw register access for bring-up and service work. Raw writes bypass the typed config helpers; use `recover()` or `begin()` to restore cached settings after manual register edits.

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
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## Behavioral Contracts

1. Threading model: single-threaded by default; not thread-safe.
2. Timing model: application code provides time via `tick(nowMs)` and must inject `Config.nowMs`; `begin()` rejects configs without it.
3. Resource ownership: bus, pins, and timeout policy remain application-owned via `Config`.
4. Memory behavior: no heap allocation in steady-state library operation.
5. Error handling: all fallible APIs return `Status`; no exceptions and no silent failures.
6. Blocking convenience APIs are bounded but not steady-path APIs.

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
