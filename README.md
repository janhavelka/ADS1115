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

The Arduino `Wire` mapping above is adapter-specific. Other frameworks may not
expose reliable address-NACK versus data-NACK detail; preserve the raw transport
detail and document any coarse mappings in the adapter.

## API Overview

### Lifecycle

| Method | Description |
|--------|-------------|
| `begin(config)` | Initialize with injected transport and verify the device is reachable |
| `tick(nowMs)` | Compatibility service step; may perform one bounded CONFIG read and suppresses the returned status |
| `service(nowMs)` | Status-returning service step for pending conversion polling work |
| `end()` | Best-effort return the ADC to single-shot idle and clear cached conversion state |
| `shutdown()` | Status-returning request to write single-shot idle while keeping the driver initialized |
| `isInitialized()` | True after successful `begin()` until `end()` |
| `getConfig()` | Return the driver's cached configuration snapshot |

### Diagnostics And Raw Access

| Method | Description |
|--------|-------------|
| `probe()` | Check device presence without updating health counters; preserves distinguishable I2C errors except definite address NACK maps to `DEVICE_NOT_FOUND` |
| `recover()` | Re-validate comms, clear conversion state, and re-apply cached config |
| `getSettings(snap)` | Populate a `SettingsSnapshot` with cached config, hook availability, and runtime state (no I2C) |
| `readRegister16(reg, value)` | Read a raw 16-bit register using tracked I2C |
| `writeRegister16(reg, value)` | Write a raw 16-bit register using tracked I2C |
| `readRegister(reg, value)` | Compatibility alias for `readRegister16()` |
| `writeRegister(reg, value)` | Compatibility alias for `writeRegister16()` |

Raw register reads accept ADS1115 registers `0x00..0x03`. Raw writes accept
only writable registers `0x01..0x03`; the conversion register `0x00` is
read-only and is rejected before I2C. Successful raw writes are diagnostic
access: they update hardware, leave the typed cache unchanged, and mark
`hardwareConfigDirty()` with `Err::HARDWARE_CONFIG_DIRTY`; the dirty diagnostic
detail is the raw register pointer. If the transport reports an error after a
raw write is attempted, the same transport status is preserved as the dirty
diagnostic because the device may still have accepted the write. Use typed
helpers such as `writeConfig()` or `setThresholds()` when the cache must stay in
sync. A later `recover()` or `begin()` clears raw-write dirty state only after
the cached settings are fully rewritten and read back successfully.

### Conversion

| Method | Description |
|--------|-------------|
| `startConversion()` | Start a single-shot conversion with the current mux; returns `Err::IN_PROGRESS` when scheduled, `Err::UNSUPPORTED_OPERATION` in continuous mode, and `Err::BUSY` only for an already-active single-shot conversion |
| `startConversion(mux)` | Start a single-shot conversion after atomically applying a temporary mux |
| `readConversionReady(ready)` | Report readiness with explicit I2C error status |
| `conversionReady(ready)` | Alias for `readConversionReady(ready)` with explicit status |
| `conversionReady()` | Convenience readiness check; `false` means not ready or an error |
| `readRaw(out)` | Read signed two's-complement conversion data |
| `readLatestRaw(out)` | Read the conversion register immediately without readiness checks |
| `readVoltage(volts)` | Read conversion data and scale it using the active gain |
| `readBlocking(out, timeoutMs)` | Start/join a single-shot conversion and wait with a finite deadline; requires `Config::nowMs`; continuous mode returns the latest register value immediately |
| `readBlockingVoltage(volts, timeoutMs)` | Blocking read with voltage scaling; requires `Config::nowMs` |

In single-shot mode, readiness is determined by ALERT/RDY when conversion-ready
pin mode is configured, otherwise by polling the OS bit after the configured
conversion time. In continuous mode `readRaw()` and `readLatestRaw()` return the
latest conversion register value immediately, which may be older than the next
data-rate interval. Use `readConversionReady()` first when the application needs
a fresh-sample indication.

`conversionReady()` is a lossy source-compatibility helper. A `false` result can
mean either "not ready" or "readiness check failed"; production code should use
`readConversionReady(bool&)` or `conversionReady(bool&)` and inspect `Status`.

`tick(nowMs)` may perform one tracked CONFIG read when a single-shot conversion
is pending and its data-rate interval has elapsed. It ignores the immediate
status for compatibility; failures are visible through `state()`, health
counters, `lastError()`, and `lastErrorMs()`. Use `service(nowMs)` when the
caller needs that immediate `Status`.

`begin()` can succeed without `Config::nowMs`. In that no-clock mode,
`SettingsSnapshot::timebaseAvailable` is false, health timestamps remain `0`,
and direct timing-based readiness checks do not advance by elapsed time. Use
`tick(nowMs)` / `service(nowMs)` from an external scheduler timebase. The
ALERT/RDY GPIO readiness path remains supported in no-clock mode once that
external service timebase has anchored and advanced the pending conversion.
Blocking reads return `INVALID_CONFIG` before starting a conversion when `nowMs`
is missing.

### Configuration

| Method | Description |
|--------|-------------|
| `setMux(mux)` | Select one of four single-ended inputs or four differential MUX selections: AIN0-AIN1, AIN0-AIN3, AIN1-AIN3, or AIN2-AIN3 |
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

Failed `begin()` calls clear stale cached configuration/runtime state. If the
failure occurs after one or more config writes may have reached hardware,
`hardwareConfigDirty()` and `hardwareConfigDirtyError()` remain available even
while `isInitialized()` is false. A successful later full `begin()` apply clears
that diagnostic.

`Config::strictInitVerify` enables an optional read-back plausibility check after
full config apply. ADS1115 has no ID register; strict mode only verifies that
threshold registers and CONFIG writable fields read back as expected. Dynamic
CONFIG OS/status bits are masked. Register read-back mismatches return
`Err::READBACK_MISMATCH` with the observed register value in `Status::detail`;
transport failures preserve the original transport status.

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

Latched comparator assertions clear when the conversion register is read or
after a successful SMBus alert response. This driver does not issue SMBus alert
response cycles.

ALERT/RDY is open-drain and requires a pull-up. In conversion-ready mode the
pulse can be short; in continuous conversion the datasheet pulse-width caveat is
approximately 8 us, so slow polling tasks may miss it. Use an interrupt-capable
GPIO, a latch strategy, or OS-bit polling when the scheduler cannot guarantee
pulse capture.

### Configuration Constraints

| Field | Valid Values |
|-------|--------------|
| `i2cWrite`, `i2cWriteRead` | Required transport callbacks |
| `i2cAddress` | ADDR to GND `0x48`, VDD `0x49`, SDA `0x4A`, or SCL `0x4B` |
| `i2cTimeoutMs` | Must be greater than zero |
| `strictInitVerify` | Optional register read-back plausibility check |
| `nowMs` | Optional for `begin()`; required by `readBlocking*()` |
| `alertRdyPin` | `-1` when unused; otherwise requires `gpioRead` |
| `offlineThreshold` | Zero is normalized to one |
| enum fields | Must be one of the documented enum values |

The datasheet says `ADDR` is sampled continuously. If `ADDR` is tied to SDA,
hold SDA low for the required setup window after SCL goes low so the address
decodes correctly. I2C and ALERT/RDY pull-up sizing is board-specific: choose
values for bus speed, total capacitance, GPIO voltage domain, sink-current
limits, and the ALERT/RDY pulse-capture strategy.

## Examples

- `examples/01_basic_bringup_cli/` - diagnostic Arduino bring-up CLI for ADS1115 features
- `examples/esp_idf/basic/` - native ESP-IDF build example with external bus context, mutex locking, timeout propagation, coarse ESP-IDF error mapping, and periodic `tick()` scheduling
- CLI address selection: `addr` prints the active ADS1115 address; `addr 0x48`,
  `addr 0x49`, `addr 0x4A`, or `addr 0x4B` reinitializes the diagnostic driver
  for that selected address. It does not automatically validate every detected
  ADS1115-range device on the bus.
- CLI register diagnostics: `reg <0..3>` and `wreg <1..3> <val>` allow raw register access for bring-up and service work. Raw writes bypass the typed config helpers, leave the typed cache unchanged, and mark hardware/cache sync dirty; use `recover()` or `begin()` to restore cached settings after manual register edits.

Current examples are diagnostic and bring-up oriented. They do not demonstrate a
production shared-bus manager with external locking, nonblocking console input,
or system-wide timeout policy. Production applications should implement those
policies in their transport callbacks.

| Example | Intent | Evidence/limits |
| --- | --- | --- |
| Arduino CLI | Diagnostic bring-up and HIL operator interface. | Reports active address, version, driver state, cached settings, and raw-write dirty diagnostics. Uses Arduino `Wire` global timeout rather than a production shared-bus manager. |
| ESP-IDF basic | Native build/integration template. | Owns bus context in the adapter, locks I2C transactions with a mutex, propagates timeout values, and uses coarse `esp_err_t` mapping. It does not prove precise address-NACK/data-NACK taxonomy; see `docs/IDF_PORT.md`. |

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
2. Timing model: `tick()`/`service()` are bounded; they may perform one tracked CONFIG read when single-shot readiness polling is due.
3. Resource ownership: bus, pins, and timeout policy remain application-owned via `Config`.
4. Memory behavior: no heap allocation in steady-state library operation.
5. Error handling: all fallible APIs return `Status`; no exceptions and no silent failures.
6. Health behavior: `OFFLINE` is latched. Normal public I2C operations return `Err::OFFLINE` with `Driver is offline; call recover()` without touching the bus until `recover()` succeeds.
7. Partial hardware state: multi-register failures can leave hardware partially updated; use `hardwareConfigDirty()` and `recover()`.
8. Electrical limits: PGA full-scale settings do not override ADS1115 analog input absolute maximum ratings (`GND - 0.3 V` to `VDD + 0.3 V`).

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
| `readBlocking()` | Single-shot start write + readiness polling + conversion read | Bounded by `timeoutMs` plus active I2C transaction timeouts; polls OS at most once per observed millisecond tick |

`readBlocking()` in continuous mode does not wait for a new sample interval; it
returns the current/latest conversion register value after the same `nowMs`
precondition check. In single-shot mode, `cooperativeYield` is called between
poll attempts when supplied. If the injected clock stops advancing, the driver
returns `Err::TIMEOUT` after a finite same-tick guard rather than spinning
forever.

## Hardware Validation Matrix

Before field release, validate at least:

| Area | Cases | Current evidence |
|------|-------|------------------|
| I2C address | ADDR straps: GND `0x48`, VDD `0x49`, SDA `0x4A`, SCL `0x4B` | Pending dated HIL log/capture |
| Modes | Single-shot, continuous, mode switching during operation | Pending dated HIL log/capture |
| Mux/gain/rate | All muxes, all PGA ranges, all data rates | Pending dated HIL log/capture |
| ALERT/RDY | Pull-up sizing/rise time, active-low/high, comparator mode, conversion-ready pulse capture | Pending dated HIL log/capture |
| Comparator | Traditional/window, latching/non-latching, queue depths, raw threshold recalculation | Pending dated HIL log/capture |
| Faults | Address NACK, data NACK, timeout, stuck bus, unplug/replug, brownout | Pending dated HIL log/capture |
| Recovery | OFFLINE latch, manual `recover()`, dirty-state clearing | Pending dated HIL log/capture |
| Platforms | Arduino ESP32-S2/S3, pure ESP-IDF adapter/build where used | Pending dated CI/HIL log/capture |

The diagnostic CLI `addr` command can select one ADS1115 address at a time for
manual validation. A bus scan showing multiple ADS1115-range addresses is not a
validation result until the operator explicitly selects each address and runs the
intended checks.

## Validation

CI-backed checks configured in `.github/workflows/ci.yml` run on `main`,
hardening branch pushes, pull requests targeting `main`, and manual
`workflow_dispatch` runs:

```bash
python tools/check_core_timing_guard.py
python scripts/generate_version.py check
pio test -e native
python tools/check_idf_example_contract.py
python tools/check_cli_contract.py
pio run -e esp32s3dev
pio run -e esp32s2dev
pio pkg pack
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Local optional checks depend on installed tools. If `idf.py` is unavailable,
record that exactly rather than reporting a pass. Hardware/HIL validation remains
pending until dated logs or captures are produced with
`docs/ADS1115_HARDWARE_VALIDATION_PLAN.md` and the results template.

## Documentation

- `CHANGELOG.md` - full release history
- `docs/IDF_PORT.md` - ESP-IDF portability guidance
- `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md` - HIL operator plan and evidence requirements
- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md` - blank results template for dated hardware runs
- `include/ADS1115/CommandTable.h` - public register map, masks, and defaults
- `docs/ADS111x_datasheet_revE.pdf` - TI datasheet copy used for driver verification
- `docs/TI_registry_reference/README.md` - TI reference-driver extraction notes

## License

MIT License. See `LICENSE`.
