# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
- `docs/IDF_PORT.md` and `docs/UNIFICATION_STANDARD.md` for standardized porting and unification guidance.
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

[Unreleased]: https://github.com/janhavelka/ADS1115/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/janhavelka/ADS1115/compare/v0.2.1...v0.3.0
[0.2.1]: https://github.com/janhavelka/ADS1115/compare/v0.2.0...v0.2.1
[0.2.0]: https://github.com/janhavelka/ADS1115/compare/v0.1.2...v0.2.0
[0.1.2]: https://github.com/janhavelka/ADS1115/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/janhavelka/ADS1115/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/janhavelka/ADS1115/releases/tag/v0.1.0
