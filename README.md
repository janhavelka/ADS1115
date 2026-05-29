# ADS1115 Driver Library

ADS1115 16-bit ADC I2C driver for ESP32-S2 / ESP32-S3 Arduino consumers,
with a framework-neutral core suitable for ESP-IDF adapters. The core library
does not own the I2C bus; production projects must provide transport callbacks
that implement their bus ownership, locking, and timeout policy.

## Features

- Injected I2C transport (no Wire dependency in library code)
- Framework-neutral core: no Arduino or ESP-IDF headers in `include/` or `src/`
- Health monitoring with READY / DEGRADED / OFFLINE states
- Single-shot and continuous conversion modes
- Configurable mux, gain, data rate, and comparator settings
- Conversion readiness through bounded OS-bit polling or ALERT/RDY pin mode
- Hardware-config-dirty diagnostics for partial multi-register writes
- Optional strict register read-back plausibility check
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

uint32_t adcNowMs(void*) {
  return millis();
}

void adcYield(void*) {
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
  cfg.nowMs = adcNowMs;              // Required by readBlocking* APIs.
  cfg.cooperativeYield = adcYield;   // Optional scheduler hint.
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
| `shutdown()` | Status-returning request to write single-shot idle while keeping the driver initialized |
| `isInitialized()` | True after successful `begin()` until `end()` |
| `getConfig()` | Return the driver's cached configuration snapshot |

### Diagnostics And Raw Access

| Method | Description |
|--------|-------------|
| `probe()` | Check device presence without updating health counters; preserves distinguishable I2C errors except definite address NACK maps to `DEVICE_NOT_FOUND` |
| `recover()` | Re-validate comms, clear conversion state, and re-apply cached config |
| `getSettings(snap)` | Populate a `SettingsSnapshot` with cached config and runtime state (no I2C) |
| `readRegister16(reg, value)` | Read a raw 16-bit register using tracked I2C |
| `writeRegister16(reg, value)` | Write a raw 16-bit register using tracked I2C |
| `readRegister(reg, value)` | Compatibility alias for `readRegister16()` |
| `writeRegister(reg, value)` | Compatibility alias for `writeRegister16()` |

Raw register reads accept ADS1115 registers `0x00..0x03`. Raw writes accept
only writable registers `0x01..0x03`; the conversion register `0x00` is
read-only and is rejected before I2C. Raw CONFIG writes accept the datasheet
PGA alias encodings `110` and `111` and normalize them to the `+/-0.256 V`
gain cache.

### Conversion

| Method | Description |
|--------|-------------|
| `startConversion()` | Start a single-shot conversion with the current mux; returns `Err::IN_PROGRESS` when scheduled |
| `startConversion(mux)` | Start a single-shot conversion after atomically applying a temporary mux |
| `readConversionReady(ready)` | Report readiness with explicit I2C error status |
| `conversionReady()` | Convenience readiness check; returns `false` on transport failure |
| `readRaw(out)` | Read signed two's-complement conversion data |
| `readLatestRaw(out)` | Read the conversion register immediately without readiness checks |
| `readVoltage(volts)` | Read conversion data and scale it using the active gain |
| `readBlocking(out, timeoutMs)` | Start/join a single-shot conversion and wait with a finite deadline; requires `Config::nowMs` |
| `readBlockingVoltage(volts, timeoutMs)` | Blocking read with voltage scaling; requires `Config::nowMs` |

In single-shot mode, readiness is determined by ALERT/RDY when conversion-ready
pin mode is configured, otherwise by polling the OS bit after the configured
conversion time. In continuous mode `readRaw()` and `readLatestRaw()` return the
latest conversion register value immediately, which may be older than the next
data-rate interval. Use `readConversionReady()` first when the application needs
a fresh-sample indication.

`begin()` can succeed without `Config::nowMs`; only `readBlocking*()` requires a
clock hook. If `nowMs` is missing, blocking reads return `INVALID_CONFIG` before
starting a conversion.

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
after the required I2C writes succeed. CONFIG-only setters use a single CONFIG
write. Full resync paths write low threshold, high threshold, and CONFIG.

If a multi-register update is partially written and then fails, the driver sets
`hardwareConfigDirty()` and preserves the original transport error in
`hardwareConfigDirtyError()`. `getSettings()` exposes the same fields. Dirty
state clears only after a successful full resync, such as `recover()` or another
successful full apply path.

Failed `begin()` calls clear stale cached configuration/runtime state, and a
successful `begin()` establishes the baseline state without inflating runtime
health counters.

`Config::strictInitVerify` enables an optional read-back plausibility check after
full config apply. ADS1115 has no ID register; strict mode only verifies that
threshold registers and CONFIG writable fields read back as expected. Dynamic
CONFIG OS/status bits are masked.

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

Comparator thresholds are signed raw conversion codes, not volts. Recalculate
threshold raw codes whenever PGA/full-scale range changes.

ALERT/RDY is open-drain and requires a pull-up. In conversion-ready mode the
pulse can be short, especially in continuous conversion; slow polling tasks may
miss it. Use an interrupt-capable GPIO, a latch strategy, or OS-bit polling when
the scheduler cannot guarantee pulse capture.

### Configuration Constraints

| Field | Valid Values |
|-------|--------------|
| `i2cWrite`, `i2cWriteRead` | Required transport callbacks |
| `i2cAddress` | `0x48`, `0x49`, `0x4A`, or `0x4B` |
| `i2cTimeoutMs` | Must be greater than zero |
| `strictInitVerify` | Optional register read-back plausibility check |
| `nowMs` | Optional for `begin()`; required by `readBlocking*()` |
| `alertRdyPin` | `-1` when unused; otherwise requires `gpioRead` |
| `offlineThreshold` | Zero is normalized to one |
| enum fields | Must be one of the documented enum values |

## Examples

- `examples/01_basic_bringup_cli/` - diagnostic Arduino bring-up CLI for ADS1115 features
- `examples/esp_idf/basic/` - native ESP-IDF build example with external bus context, mutex locking, timeout mapping, and periodic `tick()` scheduling
- CLI address selection: `addr` prints the active ADS1115 address; `addr 0x48`,
  `addr 0x49`, `addr 0x4A`, or `addr 0x4B` reinitializes the diagnostic driver
  for that selected address. It does not automatically validate every detected
  ADS1115-range device on the bus.
- CLI register diagnostics: `reg <0..3>` and `wreg <1..3> <val>` allow raw register access for bring-up and service work. Raw writes bypass the typed config helpers; use `recover()` or `begin()` to restore cached settings after manual register edits.

Current examples are diagnostic and bring-up oriented. They do not demonstrate a
production shared-bus manager with external locking, nonblocking console input,
or system-wide timeout policy. Production applications should implement those
policies in their transport callbacks.

### Example Helpers (`examples/common/`)

Not part of the library. These simulate project-level glue and keep examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Pin definitions and Wire init for supported boards |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Diagnostic Wire-based I2C transport adapter (`wireWrite`, `wireWriteRead`, `initWire`) |
| `I2cScanner.h` | Invasive diagnostic I2C scanner with table output and bus recovery |
| `BusDiag.h` | Bus diagnostics wrapper (scan + probe) |
| `CliStyle.h` | Shared ANSI colors and CLI formatting helpers |
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## Behavioral Contracts

1. Threading model: externally serialized; not thread-safe and not ISR-safe.
2. Timing model: `tick()` is bounded; conversion polling and readiness checks stay transport-timeout-bounded.
3. Resource ownership: bus, pins, and timeout policy remain application-owned via `Config`.
4. Memory behavior: no heap allocation in steady-state library operation.
5. Error handling: all fallible APIs return `Status`; no exceptions and no silent failures.
6. Health behavior: `OFFLINE` is latched. Normal public I2C operations return `BUSY` with `Driver is offline; call recover()` without touching the bus until `recover()` succeeds.
7. Partial hardware state: multi-register failures can leave hardware partially updated; use `hardwareConfigDirty()` and `recover()`.
8. Electrical limits: PGA full-scale settings do not override ADS1115 analog input absolute maximum ratings.

## I2C Transaction And Latency Model

Each transport transaction may block for up to `Config::i2cTimeoutMs`. The table
lists nominal transaction counts; strict read-back adds the noted extra reads.

| API | Transactions | Worst-case bound |
|-----|--------------|------------------|
| `begin()` | 1 CONFIG read + 3 writes; strict adds 3 reads | `4 * timeout`, strict `7 * timeout` |
| `probe()` | 1 CONFIG read | `1 * timeout` |
| `recover()` | 1 CONFIG read + 3 writes; strict adds 3 reads | `4 * timeout`, strict `7 * timeout` |
| `shutdown()` / `end()` shutdown attempt | 1 CONFIG write | `1 * timeout` |
| CONFIG-only setters | 1 CONFIG write | `1 * timeout` |
| `setThresholds()` | 2 threshold writes | `2 * timeout` |
| `getThresholds()` | 2 threshold reads | `2 * timeout` |
| `enableConversionReadyPin()` | 3 writes; strict adds 3 reads | `3 * timeout`, strict `6 * timeout` |
| `readRegister16()` / `writeRegister16()` | 1 transaction | `1 * timeout` |
| `readRaw()` / `readLatestRaw()` | 1 conversion read, plus single-shot readiness if needed | `1-2 * timeout` |
| `readBlocking()` | Start write + readiness polling + conversion read | Bounded by `timeoutMs` plus per-transaction timeouts |

## Hardware Validation Matrix

Before field release, validate at least:

| Area | Cases |
|------|-------|
| I2C address | `0x48`, `0x49`, `0x4A`, `0x4B` |
| Modes | Single-shot, continuous, mode switching during operation |
| Mux/gain/rate | All muxes, all PGA ranges, all data rates |
| ALERT/RDY | Pull-up value, active-low/high, comparator mode, conversion-ready pulse capture |
| Comparator | Traditional/window, latching/non-latching, queue depths, raw threshold recalculation |
| Faults | Address NACK, data NACK, timeout, stuck bus, unplug/replug, brownout |
| Recovery | OFFLINE latch, manual `recover()`, dirty-state clearing |
| Platforms | Arduino ESP32-S2/S3, pure ESP-IDF adapter/build where used |

The diagnostic CLI `addr` command can select one ADS1115 address at a time for
manual validation. A bus scan showing multiple ADS1115-range addresses is not a
validation result until the operator explicitly selects each address and runs the
intended checks.

## Validation

```bash
pio test -e native
python tools/check_cli_contract.py
python tools/check_core_timing_guard.py
python tools/check_idf_example_contract.py
pio run -e esp32s3dev
pio run -e esp32s2dev
pio pkg pack
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

## Documentation

- `CHANGELOG.md` - full release history
- `docs/IDF_PORT.md` - ESP-IDF portability guidance
- `include/ADS1115/CommandTable.h` - public register map, masks, and defaults
- `docs/ADS111x_datasheet_revE.pdf` - TI datasheet copy used for driver verification
- `docs/TI_registry_reference/README.md` - TI reference-driver extraction notes

## License

MIT License. See `LICENSE`.
