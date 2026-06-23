# ADS1115 HIL Validation - COM8 - 2026-06-22/23

Date/time: 2026-06-22 20:42 to 2026-06-23 05:03 Europe/Prague (UTC+02:00)  
Repository: `C:\Users\Honza\Documents\Projects\ADS1115`  
Branch/commit: `main` / `bdd36501b129cb2309a8fbeae4da24d04618f99e`  
Initial dirty status: clean. Final dirty status: implementation/report/tooling edits listed below.

## Setup

| Field | Value |
| --- | --- |
| Host OS | Microsoft Windows NT 10.0.26200.0 |
| Python | 3.12.10 |
| pySerial | 3.5 |
| PlatformIO | Core 6.1.18 |
| ESP-IDF CLI | `idf.py` not installed / not on PATH |
| Target environment | PlatformIO `esp32s2dev`, Arduino diagnostic CLI |
| Serial port | `COM8`, 115200 baud |
| Flashed board identity | ESP32-S2FH4 rev v1.0, MAC `90:e5:b1:8b:1d:60` |
| ADS1115 fixture | User reported ESP32-S2 board with two ADS1115 devices; scan found `0x48` and `0x49` |
| Electrical safety assumptions | No external stimulus, overvoltage, brownout, unplug, stuck-bus, or ALERT/RDY instrument tests were attempted |

ADS1115 has no chip-ID register. All probe results below are CONFIG-register reachability/plausibility checks, not identity proof.

## Firmware And Boot

| Step | Command | Expected | Observed | Result |
| --- | --- | --- | --- | --- |
| Build S2 | `pio run -e esp32s2dev` | Firmware builds | Success, flash 30.3%, RAM 11.2% | PASS |
| First upload | `pio run -e esp32s2dev -t upload --upload-port COM8` | Flash firmware | Failed opening COM8: Windows `PermissionError(13, 'A device attached to the system is not functioning.')` | FAIL, retried |
| Retry upload | `pio run -e esp32s2dev -t upload --upload-port COM8` | Flash firmware | Success. Esptool stayed in flasher stub because board was in boot mode. | PASS |
| Pre-reboot smoke | `python tools\run_i2c_hil.py --port COM8 ... --suite smoke --reset-before` | CLI prompt | No prompt; COM8 stayed bootloader PID `303A:0002` | FAIL, hardware state blocker |
| User reboot | Manual board reboot | Application starts | CLI responded on COM8 after reboot | PASS |

Initial failed transcript: `hil_logs\ads1115_hil_20260622_205458.log`  
Parser-adjustment failed transcript: `hil_logs\ads1115_hil_20260622_210133.log`

## Successful HIL Runs

| Run | Command | Transcript | Summary | Result |
| --- | --- | --- | --- | --- |
| Smoke gate | `python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite smoke --timeout-s 8 --idle-s 0.5 --boot-settle-s 2 --verbose --stop-on-fail` | `hil_logs\ads1115_hil_20260622_210208.log` | `hil_logs\ads1115_hil_20260622_210208_summary.md` | PASS=12, FAIL=0, UNKNOWN=2 |
| Full functional + benchmark | `python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite full --benchmark --timeout-s 8 --idle-s 0.5 --boot-settle-s 2` | `hil_logs\ads1115_hil_20260622_210237.log` | `hil_logs\ads1115_hil_20260622_210237_summary.md` | PASS=188, FAIL=0, UNKNOWN=42 |
| 8-hour soak | `python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite smoke --soak --soak-duration-s 28800 --timeout-s 8 --idle-s 0.5 --boot-settle-s 2` | `hil_logs\ads1115_hil_20260622_210327.log` | `hil_logs\ads1115_hil_20260622_210327_summary.md` | PASS=13, FAIL=0, UNKNOWN=2; soak PASS |

`UNKNOWN` rows are not hard failures. They indicate serial tokens matched, but the available fixture did not provide calibrated analog reference or external ALERT/RDY instrumentation.

## Coverage Summary

| Feature area | PASS | FAIL | UNKNOWN | Notes |
| --- | ---: | ---: | ---: | --- |
| Connectivity/address/probe/settings/health/selftest | 16 | 0 | 0 | Scan found two ADS1115-range addresses: `0x48`, `0x49` |
| MUX | 22 | 0 | 16 | All 4 single-ended and 4 differential selections exercised per address |
| Gain | 12 | 0 | 12 | All PGA enums exercised per address; voltage rows need calibrated reference |
| Data rate | 48 | 0 | 0 | All 8 data-rate enums exercised per address with bounded stress |
| Mode/conversion | 8 | 0 | 10 | Single-shot/continuous command paths exercised; analog freshness/accuracy remains fixture-limited |
| Comparator/ALERT config | 24 | 0 | 2 | Comparator settings and conversion-ready threshold mode written; ALERT/RDY pin not instrumented |
| Registers/dirty/recovery | 14 | 0 | 2 | Raw register reads/writes, dirty diagnostics, and recover exercised |
| Staged jobs | 16 | 0 | 0 | `job single`, zero budget, one-instruction poll, full-budget completion, `job apply` |
| Lifecycle/invalid input | 20 | 0 | 0 | `shutdown` and invalid CLI inputs exercised |
| Benchmarks | 8 | 0 | 0 | 50/500 scalar reads and 200-op mixed benchmark per address |
| 8-hour soak | 1 | 0 | 0 | Completed requested duration without unrecovered failure bursts |

Detailed per-step tables are in the runner summaries listed above. The full suite detailed table contains all 230 classified rows with command, expected token, observed excerpt, elapsed time, result, and notes.

## Timing And Sampling Highlights

Data-rate bounded stress at `0x48`:

| Data-rate enum | CLI conversion time | Stress count | Observed rate |
| --- | ---: | ---: | ---: |
| 0 | 130 ms | 4 | 7.62 samples/s |
| 1 | 68 ms | 4 | 14.44 samples/s |
| 2 | 37 ms | 4 | 26.32 samples/s |
| 3 | 21 ms | 4 | 44.94 samples/s |
| 4 | 10 ms | 8 | 89.89 samples/s |
| 5 | 6 ms | 8 | 142.86 samples/s |
| 6 | 4 ms | 8 | 200.00 samples/s |
| 7 | 3 ms | 8 | 242.42 samples/s |

Data-rate bounded stress at `0x49`:

| Data-rate enum | CLI conversion time | Stress count | Observed rate |
| --- | ---: | ---: | ---: |
| 0 | 130 ms | 4 | 7.62 samples/s |
| 1 | 68 ms | 4 | 14.49 samples/s |
| 2 | 37 ms | 4 | 26.14 samples/s |
| 3 | 21 ms | 4 | 44.94 samples/s |
| 4 | 10 ms | 8 | 89.89 samples/s |
| 5 | 6 ms | 8 | 140.35 samples/s |
| 6 | 4 ms | 8 | 195.12 samples/s |
| 7 | 3 ms | 8 | 242.42 samples/s |

Benchmark mode:

| Address | Command | Count | Duration | Rate | Errors |
| --- | --- | ---: | ---: | ---: | ---: |
| `0x48` | `stress 50` | 50 | 161 ms | 310.56 samples/s | 0 |
| `0x48` | `stress 500` | 500 | 1510 ms | 331.13 samples/s | 0 |
| `0x48` | `stress_mix 200` | 200 ops | 2418 ms | 82.71 ops/s | 0 |
| `0x49` | `stress 50` | 50 | 161 ms | 310.56 samples/s | 0 |
| `0x49` | `stress 500` | 500 | 1511 ms | 330.91 samples/s | 0 |
| `0x49` | `stress_mix 200` | 200 ops | 2418 ms | 82.71 ops/s | 0 |

## 8-Hour Soak

| Field | Observed |
| --- | --- |
| Start | 2026-06-22 21:03:38 Europe/Prague |
| End | 2026-06-23 05:03:37 Europe/Prague |
| Requested duration | 28800 s |
| Actual runner duration | 28799.433 s |
| Cycles | 16296 |
| Commands | 717010 |
| Per-command classifications | PASS=554051, UNKNOWN=162959, FAIL=0 |
| Worst command latency | 0.485 s |
| Mean command latency | 0.039 s |
| Stop reason | Completed requested duration |

The soak command mix cycled both addresses through reads, voltage reads, continuous/raw reads, rate/gain boundaries, short stress blocks, staged single-shot jobs, settings, health, probe, and recover. No failure burst, serial reconnect, target reboot, offline state, or persistent transport error was observed by the runner.

## Tooling Changes Made

- Extended `tools/run_i2c_hil.py` with repeated `--address`, bounded `--timeout-s`, `--idle-s`, `--boot-settle-s`, `--reset-before`, verbose transcript capture, dry-run/parser self-test, classified full suite, benchmark mode, and bounded soak mode.
- Added Arduino diagnostic CLI commands: `job`, `job single`, `job apply`, `job poll [0..255]`, `job cancel`, and `shutdown`.
- Added matching native ESP-IDF diagnostic CLI commands for staged jobs and `shutdown`.
- Updated CLI contract scripts to require the new diagnostic commands.
- Script adjustments during HIL:
  - Tightened generic failure-token matching so `fail=0` in selftest output is not a failure.
  - Fixed soak-plan generation so multiple addresses do not duplicate soak steps.
  - Accepted the observed scan table format (`48 49`) as ADS1115-range address evidence, not only `0x48` text.

## Repository Audit Findings

| Severity | Finding | Evidence | Risk | Proposed fix | Implemented |
| --- | --- | --- | --- | --- | --- |
| High | Active poll jobs can be interleaved with other mutating APIs | Code audit: `startApplyConfigJob()` snapshots registers, while setters/raw writes/shutdown/startConversion/readBlocking do not reject `_jobActive` | Cache and hardware can diverge with dirty state clear | Add shared `_jobActive` guard to mutating public APIs except matching poll/cancel/accessors; add native and HIL interleaving tests | No |
| Medium | `startApplyConfigJob()` is effectively unavailable in normal continuous mode | Code audit: continuous mode sets `_conversionStarted=true`; `startApplyConfigJob()` rejects `_conversionStarted` | Users in continuous mode fall back to blocking multi-transaction apply | Treat `_conversionStarted` as blocking only for active single-shot conversions; test continuous apply job | No |
| Medium | First poll-single-shot config-write failures mark dirty even for definite non-write failures | Code audit: `_failJob()` marks dirty unconditionally for `SINGLE_SHOT_WRITE_CONFIG` failures | Misleading dirty diagnostics after address NACK/preflight failure | Use `_markHardwareConfigDirtyIfClean()` for first single-shot job write; test NACK vs timeout | No |
| Low/Medium | Readback mismatch can leave health `READY` while config dirty | Code audit: tracked readback success resets health before `READBACK_MISMATCH` sets dirty | HIL gates that check only READY can miss config mismatch | Require HIL/CLI pass criteria to include clean hardware/cache state, or define explicit operational predicate | Partially: runner settings checks require clean dirty state |
| Medium | Existing live HIL runner was one-address and shallow | Tooling audit and original `run_i2c_hil.py` | Two-device fixture and staged APIs were not covered | Extend runner for repeated addresses, functional suites, benchmarks, soak | Yes |
| Low | ESP-IDF example remains single-address | `examples/esp_idf/basic/main/main.cpp` uses fixed `ADS1115_I2C_ADDR` | ESP-IDF HIL cannot validate both simultaneous ADS1115 devices without rebuild/edit | Document as limitation or add carefully scoped IDF address selection later | Documented here only |

Post-report follow-up on 2026-06-23: current source now includes host-tested
fixes for active poll-job interleaving, continuous-mode staged apply, precise
first single-shot dirty marking, and ESP-IDF `addr <0x48..0x4B>` CLI parity.
Those fixes are not part of the 2026-06-22 COM8 HIL evidence above and require
a fresh clean-firmware targeted HIL run before they can be claimed as hardware
validated.

## Validation Run

| Command | Result |
| --- | --- |
| `python tools\run_i2c_hil.py --parser-test` | PASS |
| `python tools\run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite smoke` | PASS |
| `python tools\run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite full --benchmark` | PASS |
| `python tools\check_cli_contract.py` | PASS |
| `python tools\check_idf_example_contract.py` | PASS |
| `python scripts\generate_version.py check` | PASS |
| `python tools\check_core_timing_guard.py` | PASS |
| `pio test -e native` | PASS, 130/130 tests |
| `pio run -e esp32s2dev` | PASS |
| `pio run -e esp32s3dev` | PASS |
| `pio pkg pack` | PASS, generated archive removed after check |
| `git diff --check` | PASS |
| `idf.py --version` | NOT RUN, `idf.py` not found on PATH |

## Files Changed

- `examples/01_basic_bringup_cli/main.cpp`
- `examples/esp_idf/basic/main/main.cpp`
- `tools/check_cli_contract.py`
- `tools/check_idf_example_contract.py`
- `tools/run_i2c_hil.py`
- `docs/reports/hil-validation-COM8-20260622.md`

## Limitations

- No production-readiness claim is made.
- No calibrated analog source or DMM readings were available, so raw/voltage plausibility rows remain `UNKNOWN`.
- No oscilloscope or logic-analyzer capture was available for ALERT/RDY pulse width, polarity, latch behavior, or I2C waveform validation.
- No unsafe fault injection was attempted: no unplug/replug, stuck bus, brownout, overvoltage, overtemperature, or power-cycle stress.
- ESP-IDF example hardware validation was not run because `idf.py` is not installed and the IDF example currently targets one fixed address per build.
- CI service status was not checked; only local commands listed above were run.
