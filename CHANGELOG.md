# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0] - 2026-03-01

### Changed
- Unified `01_basic_bringup_cli` command behavior and operator-facing output clarity.
- Updated `docs/IDF_PORT.md` to reflect current timing-hook architecture and no direct core timing calls in driver core.

### Fixed
- CLI reporting consistency for probe/stress/health paths in the bringup example.

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

[Unreleased]: https://github.com/janhavelka/ADS1115/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/janhavelka/ADS1115/compare/v0.1.2...v0.2.0
[0.1.1]: https://github.com/janhavelka/ADS1115/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/janhavelka/ADS1115/releases/tag/v0.1.0
