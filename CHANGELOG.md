# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- ESP-IDF component metadata, generated-version CMake support, and a basic
  ESP-IDF `i2c_master` example.
- `SettingsSnapshot` struct for reading cached configuration and runtime state without I2C.
- `getSettings(SettingsSnapshot&)` method to populate a settings snapshot.
- `Status::is(Err)` method for type-safe error code comparison.
- `Status::operator bool()` explicit conversion for concise success checks.
- `readRegister()` and `writeRegister()` compatibility aliases for `readRegister16()` / `writeRegister16()`.
- `readConversionReady(bool&)` for conversion readiness checks with explicit transport error reporting.
- Datasheet PGA alias handling for raw CONFIG writes: encodings `110` and `111` are accepted as `+/-0.256 V`.
- Native coverage for register-modeled conversion reads, readiness failures, ALERT/RDY readiness, setter rollback, register validation, and stalled-clock blocking timeouts.
- Native coverage proving latched `OFFLINE` blocks normal I2C operations without touching the bus while `recover()` remains the explicit recovery path.

### Changed
- Driver core timing/yield ownership moved fully behind application callbacks;
  Arduino examples now provide explicit timing hooks instead of relying on core fallbacks.
- Doxyfile project metadata now matches `library.json`, and archived prompt
  metadata no longer contains placeholder ownership values.
- Reference documentation now uses a human-readable vendor PDF name and separates compact chip notes from full PDF extraction under `docs/extracted-md/` and `docs/pdf-extracted-md/`.
- Explicit recovery bypass internals now use the shared `ScopedOfflineI2cAllowance` / `_reassertOfflineLatch()` procedure so failed recovery attempts that begin from `OFFLINE` keep the latch asserted.
- Continuous-mode readiness now tracks the configured data-rate interval instead of reporting ready immediately.
- CLI `poll`, `selftest`, and mixed stress paths now handle `Err::IN_PROGRESS` correctly and preserve readiness I2C errors.
- README now documents conversion, configuration, comparator, ALERT/RDY, and configuration constraint APIs.
- `begin()` failure now clears stale cached configuration/runtime state, and successful startup no longer seeds runtime health counters.
- Health behavior is now standardized on latched `OFFLINE`: normal public I2C operations return `BUSY` with `Driver is offline; call recover()` and do not touch I2C until `recover()` succeeds.

### Fixed
- Typed config and comparator setters no longer commit cached configuration changes when their I2C writes fail.
- Raw register helpers now reject pointers outside the ADS1115 `0x00..0x03` map, and reject conversion-register writes to read-only `0x00`, before touching the bus.
- Example diagnostic error strings now include granular `I2C_*` status codes.
- `readBlocking()` now has a finite escape path if an injected clock hook stops advancing.

## [1.0.0] - 2026-04-05

### Changed
- Promoted to v1.0.0 — the library is fully featured and production-ready.

## [0.4.0] - 2026-04-05

### Added
- Public lifecycle introspection helpers: `isInitialized()` and `getConfig()`.
- Public tracked raw-register helpers: `readRegister16()` and `writeRegister16()`.
- `Err::MEASUREMENT_NOT_READY` alias for cross-library uniformity.

### Changed
- `end()` now best-effort returns the ADC to single-shot idle and clears cached conversion state.
- `recover()` now clears conversion state and re-applies cached configuration after the tracked probe succeeds.
- Bringup CLI now exposes `reg` / `wreg` diagnostics for raw register inspection and service writes.

## [0.3.0] - 2026-04-03

### Added
- Granular I2C transport status codes: `I2C_NACK_ADDR`, `I2C_NACK_DATA`, `I2C_TIMEOUT`, and `I2C_BUS`.

### Changed
- Documented the newer transport contract in the README, including granular `I2C_*` status mapping, `i2cUser` usage, and manager-owned timeout guidance.
- `examples/common/I2cTransport.h` now uses `TwoWire*` via `i2cUser`, validates buffer parameters, and treats per-call `timeoutMs` as advisory.
- Added native coverage for raw transport parameter validation.

### Fixed
- `_i2cWriteRaw()` and `_i2cWriteReadRaw()` now reject null and zero-length transactions before calling the injected transport.

## [0.2.1] - 2026-04-03

### Added
- `CommandHandler.h` example helper for serial command parsing (`cmd::readLine`, `cmd::match`, `cmd::parseInt`).
- `HealthDiag.h` example helper with verbose health diagnostics, color-coded output, snapshots, diffs, and `HealthMonitor` class for continuous monitoring.

### Changed
- `I2cScanner.h` upgraded: advanced table-format scan, bus recovery via `recoverBus()`, timeout support, yield() calls, common address hints, `LOG_SERIAL` macro usage.
- `I2cTransport.h` upgraded: `TwoWire*` via user pointer (no global Wire), null pointer checks, 128-byte buffer validation, detailed per-error-code mapping, `ARDUINO_ARCH_ESP32` guards.
- `BusDiag.h` updated to use `i2c_scanner::scan(Wire)` and include `<Wire.h>`.
- `Log.h`: added `LOGV` runtime-verbose macro, ESP32-S3 USB CDC delay in `log_begin()`.
- `BuildConfig.h`: added Doxygen header comment block.
- `main.cpp` example updated to use `bus_diag::scan()`, set `cfg.i2cUser = &Wire`, corrected includes.

## [0.2.0] - 2026-03-01

### Changed
- Unified `01_basic_bringup_cli` command behavior and operator-facing output clarity.
- Updated `docs/IDF_PORT.md` to reflect current timing-hook architecture and no direct core timing calls in driver core.

### Fixed
- CLI reporting consistency for probe/stress/health paths in the bringup example.

## [0.1.2] - 2026-02-28

### Added
- Unified example framework helpers under `examples/common/*` (`BuildConfig`, `BusDiag`, `CliShell`, `HealthView`, `TransportAdapter`).
- `docs/IDF_PORT.md` for standardized ESP-IDF portability guidance.
- CLI/timing contract checks via `tools/check_cli_contract.py` and `tools/check_core_timing_guard.py`.

### Changed
- Consolidated example workflow to canonical `examples/01_basic_bringup_cli`.
- Aligned CI/test layout to the shared unification profile (`test/test_basic.cpp`, platform and workflow updates).

## [0.1.1] - 2026-02-22

### Fixed
- `readBlocking()` tight CPU spin loop without `yield()` — could trigger ESP32 watchdog timeout at low data rates (up to 130 ms at 8 SPS)
- `readBlocking()` stale deadline calculation when joining an already-running conversion (BUSY path)
- `readBlocking()` not clearing `_conversionStarted` on timeout — caused all subsequent `startConversion()` calls to permanently return BUSY
- `int16_t compThresholdLow` default `0x8000` narrowing conversion — changed to portable `-32768`
- `enableConversionReadyPin()` same narrowing fix for high threshold assignment
- Example CLI unbounded serial input buffer — capped at 128 characters to prevent heap exhaustion
- Example CLI unbounded `read N` / `stress N` iteration count — capped at 10 000 / 100 000
- Test `RUN_TEST` macro missing `try` block — was a syntax error
- Test assertions used `assert()` which calls `abort()` — replaced with throwing macros that report file and line
- Test Arduino.h stub missing `#include <string>` and `yield()` stub
- Test Wire.h stub `write(uint8_t)` buffer overflow — added bounds check
- Test Wire.h stub `write(buf, len)` returned `len` instead of actual bytes written
- Test Wire.h stub `available()` unsigned underflow when `_rxIdx > _rxLen`

## [0.1.0] - 2026-01-19

### Added
- ADS1115 driver with injected I2C transport and health tracking
- Single-shot and continuous conversion APIs with voltage helpers
- Comparator configuration and ALERT/RDY support
- Bringup CLI example for ESP32-S2 / ESP32-S3

[Unreleased]: https://github.com/janhavelka/ADS1115/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/janhavelka/ADS1115/compare/v0.4.0...v1.0.0
[0.4.0]: https://github.com/janhavelka/ADS1115/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/janhavelka/ADS1115/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/janhavelka/ADS1115/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/janhavelka/ADS1115/compare/v0.1.2...v0.2.0
[0.1.2]: https://github.com/janhavelka/ADS1115/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/janhavelka/ADS1115/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/janhavelka/ADS1115/releases/tag/v0.1.0
