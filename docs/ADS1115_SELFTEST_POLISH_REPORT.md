# ADS1115 Selftest Polish Report

Date: 2026-05-29
Branch at original pass: `hardening/ads1115-industry-readiness`
Current hardening continuation: `hardening/ads1115-industry-standard-p0`. See
`docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` for current Prompt 06
validation.

## Summary

This follow-up polish pass fixes the CLI `selftest` sequencing issue that could
report:

```text
[FAIL] readVoltage - CONVERSION_NOT_READY
```

The failure was a selftest bug, not a core driver bug. In single-shot mode,
`readRaw()` consumes the ready/fresh state. A later nonblocking `readVoltage()`
correctly calls `readRaw()` again and returns `CONVERSION_NOT_READY` if no fresh
conversion has been started.

The CLI selftest now validates raw and voltage reads independently by starting
and polling a separate single-shot conversion before the nonblocking
`readVoltage()` check.

## Code Changes

- Updated `examples/01_basic_bringup_cli/main.cpp`:
  - Added independent `startConversion(raw)` and `startConversion(voltage)`
    paths in `selftest`.
  - Added bounded selftest readiness polling helper.
  - Added selftest failure context with operation, status code, detail, message,
    mode, rate, mux, and whether conversion-ready was expected.
  - Restores baseline mode, mux, gain, data rate, comparator mode, comparator
    polarity, comparator latch, comparator thresholds, and comparator queue.
  - Added diagnostic `addr` command:
    - `addr`
    - `addr 0x48`
    - `addr 0x49`
    - `addr 0x4A`
    - `addr 0x4B`
  - Added active address reporting for `cfg`, `state`, `probe`, `read`,
    `readv`, `raw`, `voltage`, `stress`, `stress_mix`, and `selftest`.
  - Startup now prints firmware/library version information and ESP32 reset
    reason when available.
- Updated `README.md`:
  - Documented the diagnostic `addr` command.
  - Clarified that scan results for multiple ADS1115-range addresses are not
    validation results until each selected address is explicitly tested.
- Updated `tools/check_cli_contract.py` to require the `addr` command.
- Added native regression coverage in `test/test_basic.cpp` proving that a
  single-shot `readRaw()` consumes readiness before a later nonblocking
  `readVoltage()`.

The Arduino CLI command loop itself is not directly host-tested in the native
suite because it is an Arduino `Serial` sketch. The native test covers the core
contract that caused the CLI sequencing bug.

## Address Selection

Address selection did not previously exist in the CLI. It was added as a
diagnostic convenience only. It does not change the core driver API and does not
automatically validate every detected address.

The current default selected address remains `0x48`.

## Local Validation

Commands run:

```text
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
idf.py --version
python -m platformio device list
```

Results from the original 2026-05-29 local pass:

```text
Core timing guard PASSED
CLI contract PASSED
IDF example contract PASSED
Version.h up to date
Native tests PASSED: 57 test cases, 57 succeeded
Arduino ESP32-S3 build SUCCESS
Arduino ESP32-S2 build SUCCESS
idf.py --version FAILED: idf.py not found in this local shell
PlatformIO device list completed
```

The historical native count above is not current. Prompt 06 on
`hardening/ads1115-industry-standard-p0` reports 106/106 native test cases.

Pure ESP-IDF local builds were not run because `idf.py` is not installed or not
on `PATH` in this shell.

## Hardware Validation

Requested hardware command sequence:

```text
version
scan
addr
cfg
state
selftest
stress 500
stress_mix 200
rate 2
stress 50
rate 1
stress 50
```

Requested address sanity sequence:

```text
addr 0x48
probe
cfg
addr 0x49
probe
cfg
addr 0x48
```

These hardware commands were not run in this session. Serial discovery found
USB serial devices, but `COM15` and `COM16` returned access denied, and `COM5`
opened but did not respond to a `version` command at 115200 baud. No dated
hardware log or capture path is recorded here for `stress 500` or
`stress_mix 200`, so those runs are not claimed as validation evidence.

## Remaining Limitations

- Fresh hardware selftest and stress runs are still pending after this change.
- Address `0x49` was not validated by this session.
- Pure ESP-IDF local build remains pending until an ESP-IDF environment with
  `idf.py` is available.
- Physical fault paths remain untested here: address NACK on real hardware, data
  NACK, stuck bus, unplug/replug, brownout, and ALERT/RDY pulse capture under
  scheduler load.

## Industry-Readiness Assessment

This pass moves the library and diagnostic tooling closer to industry-grade by
removing a misleading selftest failure mode, documenting the underlying
single-shot freshness contract with a native regression test, restoring hardware
settings after selftest, and making selected I2C address state explicit in the
CLI. The remaining gaps are validation coverage, not known core-driver defects.
