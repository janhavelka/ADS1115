# ADS1115 Industry-Readiness Hardening Final Report

Date: 2026-05-29
Branch: `hardening/ads1115-industry-readiness`

## Summary

This branch hardens the ADS1115 driver against the industry-readiness findings:
framework neutrality, explicit timing contracts, partial hardware-state
diagnostics, improved probe errors, strict read-back verification, continuous
read semantics, shutdown reporting, thread/ISR contracts, example honesty, and
expanded CI/build coverage.

The library is now closer to industry-grade. Remaining gaps are primarily
hardware validation and local execution of pure ESP-IDF builds in an environment
with ESP-IDF installed.

## Public API Changes

- Added `Config::strictInitVerify`.
- Added `SettingsSnapshot::strictInitVerify`.
- Added `SettingsSnapshot::hardwareConfigDirty`.
- Added `SettingsSnapshot::hardwareConfigDirtyError`.
- Added `ADS1115::shutdown()`.
- Added `ADS1115::hardwareConfigDirty()`.
- Added `ADS1115::hardwareConfigDirtyError()`.
- Added `ADS1115::readLatestRaw()`.
- Deleted copy and move operations for `ADS1115`.
- Changed `readBlocking()` and `readBlockingVoltage()` to require
  `Config::nowMs`; they return `INVALID_CONFIG` before starting conversion when
  no clock hook is configured.
- Changed `probe()` mapping: definite address NACK maps to `DEVICE_NOT_FOUND`;
  timeout, bus, data NACK, and generic I2C errors are preserved.

## Core Changes

- Removed Arduino dependency from `src/ADS1115.cpp`; missing timing hooks now
  produce framework-neutral behavior.
- Made `_cooperativeYield()` a no-op unless the application supplies a yield
  callback.
- Added hardware-config-dirty tracking for partial multi-register writes.
- Preserved the original dirtying `Status` for diagnostics.
- Cleared dirty state only after successful full config resync.
- Added optional strict read-back verification for CONFIG and threshold
  registers, with CONFIG OS/status bits masked.
- Optimized CONFIG-only setters to a single CONFIG write.
- Documented `end()` as best-effort and added `shutdown()` for callers that need
  an explicit write result.
- Clarified continuous mode: `readRaw()` and `readLatestRaw()` return the latest
  conversion register value immediately; `readConversionReady()` is the
  fresh-sample indication path.

## Tests Added

Native fake-transport coverage now includes:

- Missing `nowMs` for blocking raw and voltage reads.
- Partial write failure at first, second, and third full-apply transaction
  positions.
- Dirty-state exposure through direct API and `SettingsSnapshot`.
- Dirty-state clearing after successful full recover.
- CONFIG-only setter not clearing prior threshold dirty state.
- Probe mapping for address NACK, timeout, bus, data NACK, and generic I2C
  errors.
- Strict read-back success with OS-bit masking.
- Strict read-back mismatch failure without initialization.
- Strict recover read-back success and mismatch behavior.
- Continuous latest-register semantics.
- `shutdown()` success, transport error, and offline behavior.
- `end()` best-effort behavior when shutdown write fails.
- Compile-time copy/move prevention.

Native test result:

```text
56 test cases: 56 succeeded
```

## Documentation And Examples

- Updated `AGENTS.md` with hardening rules and subagent roles.
- Updated README with:
  - `nowMs` blocking-read contract.
  - dirty-state diagnostics.
  - strict read-back limitations.
  - transaction/latency table.
  - external serialization and non-ISR-safe contract.
  - continuous latest-versus-fresh semantics.
  - ALERT/RDY pull-up and pulse-capture warning.
  - PGA/full-scale input warning.
  - comparator threshold raw-code warning.
  - hardware validation matrix.
  - diagnostic-only honesty for current examples.
- Updated `docs/IDF_PORT.md` to match the framework-neutral core and blocking
  timing contract.
- Marked Arduino helper/scanner code as diagnostic, not production shared-bus
  glue.
- Added explicit Arduino timing callbacks to the Arduino bring-up example.
- Added a minimal native ESP-IDF example at `examples/esp_idf/basic` with:
  - `app_main`.
  - `driver/i2c_master.h`.
  - `esp_timer`.
  - FreeRTOS delay/yield.
  - external bus context.
  - mutex-based bus locking.
  - timeout/error mapping.
  - periodic nonblocking `tick()` scheduling.

## CI And Build Coverage

Updated CI to cover:

- Native tests.
- Arduino ESP32-S2 build.
- Arduino ESP32-S3 build.
- Guard scripts.
- Package validation.
- Pure ESP-IDF example builds for `esp32s2` and `esp32s3` through the ESP-IDF
  CI container.

Added:

- Root ESP-IDF component metadata: `CMakeLists.txt`, `idf_component.yml`.
- ESP-IDF example contract guard:
  `tools/check_idf_example_contract.py`.

## Checks Run Locally

All available local checks run in this workspace:

```text
python tools/check_core_timing_guard.py
PASS

python tools/check_cli_contract.py
PASS

python tools/check_idf_example_contract.py
PASS

python scripts/generate_version.py check
PASS: Version.h up to date

python -m platformio test -e native
PASS: 56/56 tests

python -m platformio run -e esp32s3dev
PASS
RAM: 22304 bytes / 327680 bytes
Flash: 395318 bytes / 1310720 bytes

python -m platformio run -e esp32s2dev
PASS
RAM: 36752 bytes / 327680 bytes
Flash: 387201 bytes / 1310720 bytes

python -m platformio pkg pack
PASS
```

The package command produced `ADS1115-1.0.0.tar.gz`; the generated tarball was
removed after validation to avoid leaving build artifacts in the worktree.

## Checks Not Run Locally

Pure ESP-IDF builds were added to CI but not run locally because this shell does
not have `idf.py` or Docker available:

```text
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

These should be verified in CI or on a machine with ESP-IDF installed.

## Remaining Hardware Validation

Hardware validation is still required before claiming field-grade readiness:

- All I2C addresses: `0x48`, `0x49`, `0x4A`, `0x4B`.
- Single-shot and continuous modes.
- All mux, gain, and data-rate settings.
- ALERT/RDY active-low and active-high behavior with real pull-ups.
- Conversion-ready pulse capture under the target scheduler.
- Comparator traditional/window, latch, polarity, queue, and threshold behavior.
- PGA/full-scale use while respecting analog absolute maximum limits.
- Address NACK, data NACK, timeout, stuck bus, unplug/replug, and brownout.
- OFFLINE latch and manual `recover()` behavior.
- Hardware-config-dirty behavior after induced partial writes.
- ESP-IDF adapter behavior on actual ESP32-S2/S3 boards.

## Readiness Assessment

The library is materially closer to industry-grade:

- Core framework neutrality is enforced by guard script.
- Blocking timing assumptions are enforced in code.
- Partial hardware state is observable and recoverable.
- Diagnostic error mapping is more precise.
- Public API contracts are clearer.
- Native tests cover the new fault cases.
- ESP-IDF build coverage is represented in CI.

The remaining blockers are not ordinary unit-test gaps; they are physical-bus and
platform validation tasks.
