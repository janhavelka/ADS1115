# ADS1115 Industry-Standard Hardening Final Report

Date: 2026-06-01
Branch: `hardening/ads1115-industry-standard-p0`
Starting commit: `65f6fdcc5c7b1d4da2d94eec5e4393614598e3f7`
Current head before this final report: `c4558b293a66975112a0db85c10a2cd8dde814b4`
Final commit: report commit containing this file; the immutable hash is recorded in the final assistant response.

## Executive Summary

This hardening pass made the ADS1115 driver more production-oriented without
redesigning the library. The core remains framework-neutral, I2C remains
injected and non-owning, public failure paths now expose precise statuses, and
native fault coverage expanded to 106 passing tests.

Local validation passed for all available checks, native tests, Arduino
ESP32-S2/S3 PlatformIO builds, and PlatformIO package packing. Local pure
ESP-IDF builds were not run because `idf.py` is unavailable in this shell.

The branch is not merge-ready directly against current `origin/main`: read-only
`git merge-tree --write-tree HEAD origin/main` exited 1 and reported conflicts.
Merge is recommended only after rebasing or merging current `origin/main`,
resolving conflicts, and rerunning the validation set.

The repository is not ready for production or field-grade release claims because
dated hardware/HIL logs and captures are still pending. It is ready for the user
to begin hardware validation using the prepared plan, results template, and
capture helper.

## What Changed By Chunk

Prompt 01 initialized the hardening implementation branch, locked rules in
`AGENTS.md`, and created the implementation tracking report.

Prompt 02 added test-first contracts for failed `begin()` partial state,
strict read-back mismatch status, offline/unsupported/dirty taxonomy, and raw
diagnostic register writes.

Prompt 03 implemented P0 status taxonomy, strict read-back mismatch handling,
offline short-circuit behavior, continuous-mode unsupported-operation status,
and failed-`begin()` dirty diagnostics.

Prompt 04 closed the raw register write contract: public raw writes to CONFIG,
LO_THRESH, and HI_THRESH are diagnostic writes that mark hardware/cache sync
dirty, while conversion-register writes are rejected as read-only.

Prompt 05 clarified readiness and timing contracts. The bool-only
`conversionReady()` remains a compatibility convenience, status-returning
readiness APIs preserve transport errors, `service(nowMs)` was added next to
`tick(nowMs)`, no-clock behavior is explicit, and blocking read polling is
bounded by observed time and cooperative yield hooks.

Prompt 06 expanded native edge coverage, strengthened core leakage guards,
added version metadata checking, and corrected documentation wording so
hardware validation remains pending instead of implied.

Prompt 07 clarified Arduino diagnostic CLI and ESP-IDF example evidence,
documented ESP-IDF error mapping limits, expanded CI configuration, and added
HIL validation plan/template plus `tools/hil_ads1115_capture.py`.

Prompt 08 produced this final readiness report and closed the implementation
report. Only documentation/comment consistency fixes were added.

## Public API / Status Changes

Appended status values without reordering existing enum members:

- `Err::OFFLINE`
- `Err::UNSUPPORTED_OPERATION`
- `Err::READBACK_MISMATCH`
- `Err::HARDWARE_CONFIG_DIRTY`

Added or exposed public APIs and diagnostics:

- `Status conversionReady(bool& ready)` as a status-returning alias.
- `Status readConversionReady(bool& ready)` documented as the production
  readiness path.
- Existing `bool conversionReady()` kept as a convenience API that returns false
  for both not-ready and error.
- `Status service(uint32_t nowMs)` for status-returning periodic service.
- Existing `void tick(uint32_t nowMs)` kept for compatibility and delegates to
  service behavior while surfacing failures through health/last-error state.
- `Status shutdown()` for explicit best-effort single-shot idle handling.
- `Status readLatestRaw(int16_t& out)` for continuous-mode latest-register reads.
- `bool hardwareConfigDirty() const` and
  `Status hardwareConfigDirtyError() const` diagnostics.
- `SettingsSnapshot` and `getSettings(SettingsSnapshot&)` include runtime/cache
  state without I2C.
- `Config::strictInitVerify` enables optional writable-register read-back
  plausibility checks.

## Compatibility Notes

The status enum additions are append-only. Existing numeric values are preserved.

The bool-only readiness and void tick APIs remain available for source
compatibility, but production callers should prefer status-returning APIs.

Some callers may observe more precise statuses than before. For example,
continuous-mode `startConversion()` now returns `UNSUPPORTED_OPERATION` instead
of generic `BUSY`, offline public I2C paths return `OFFLINE`, and strict
read-back mismatches return `READBACK_MISMATCH`.

Release metadata remains `1.0.0`; do not tag or publish this work as another
`1.0.0`. A future release should be at least `1.1.0` for the backward-compatible
API/status additions, or `2.0.0` only if downstream consumers treat exact status
changes as breaking.

The public API remains not ISR-safe and driver instances remain not internally
thread-safe unless the application serializes calls externally.

## Datasheet Contract Review

No chip-ID claim was found. Docs state ADS1115 has no ID register and strict
startup is writable-register plausibility/read-back only.

Address handling covers the four ADS1115 ADDR strap values: `0x48`, `0x49`,
`0x4A`, and `0x4B`.

MUX wording is corrected to four single-ended input selections and four
differential MUX selections. It does not claim four independent differential
channels.

PGA documentation distinguishes full-scale range from absolute analog input
limits and keeps the overvoltage warning.

Register map handling is bounded to ADS1115 pointer values `0x00..0x03`; raw
writes reject conversion register `0x00` as read-only.

Continuous mode is documented as returning the current/latest conversion
register value; `readBlocking()` is not claimed to wait for a newly completed
sample in continuous mode.

ALERT/RDY docs cover open-drain pull-up requirements and the short continuous
conversion pulse caveat of approximately 8 us.

Comparator latch behavior is documented: latched comparator assertions clear on
conversion-register read or successful SMBus alert response. The driver does not
issue SMBus alert response cycles.

No silent general-call reset use was found in core or examples.

## Tests Added/Changed

Native tests now cover:

- Append-only status taxonomy.
- Failed `begin()` after partial apply writes.
- Strict read-back mismatches and read-back transport failures.
- Offline short-circuit behavior with no bus access.
- Continuous-mode unsupported operation.
- Raw CONFIG/threshold diagnostic dirty marking and recovery clearing.
- Invalid address, enum, ALERT/RDY, and raw register boundaries.
- Distinct tracked transport statuses: `I2C_NACK_ADDR`, `I2C_NACK_DATA`,
  `I2C_TIMEOUT`, `I2C_BUS`, and `I2C_ERROR`.
- Threshold sign reconstruction and boundary values.
- `rawToVoltage()` and `getLsbVoltage()` across gains.
- Setter rollback and dirty-state preservation variants.
- Readiness API error preservation, service/tick failure surfacing, no-clock
  diagnostics, and bounded blocking read polling.
- Shutdown/end and offline behavior.

Prompt 08 final-state native result: 106 test cases, 106 succeeded.

## Commands Run And Exact Results

Startup/diff review:

| Command | Result |
| --- | --- |
| `git status --short` | Clean before Prompt 08 edits |
| `git log --oneline --decorate -10` | Head was `c4558b2 (HEAD -> hardening/ads1115-industry-standard-p0, origin/hardening/ads1115-industry-standard-p0) docs: add ADS1115 integration and HIL validation plan`; previous hardening commits `73f87fe`, `3f4583b`, `8056c96`, `effddc5`, `7ce66d1`, `38e55ea`; base branch commit `65f6fdc` |
| `git diff --stat` | No output before Prompt 08 edits |
| `git diff --name-only origin/main...HEAD` | Succeeded; 33 files changed relative to merge-base |
| `git merge-base origin/main HEAD` | `73569c4826607aa9b09a58f592f9cd9391bf8c1e` |
| `git merge-tree --write-tree HEAD origin/main` | Exit code 1. Reported conflicts in `.github/workflows/ci.yml`, `AGENTS.md`, `CHANGELOG.md`, `CMakeLists.txt`, `README.md`, `docs/IDF_PORT.md`, `examples/01_basic_bringup_cli/main.cpp`, `examples/esp_idf/basic/CMakeLists.txt`, `examples/esp_idf/basic/main/CMakeLists.txt`, `examples/esp_idf/basic/main/main.cpp`, `idf_component.yml`, `include/ADS1115/Config.h`, `tools/check_core_timing_guard.py`, and `tools/check_idf_example_contract.py` |

Validation:

| Command | Result |
| --- | --- |
| `python --version` | `Python 3.13.12` |
| `python -m platformio --version` | `PlatformIO Core, version 6.1.19` |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `native` passed; `106 test cases: 106 succeeded in 00:00:02.166` |
| `python -m platformio run -e esp32s3dev` | `esp32s3dev` success in `00:00:14.457`; RAM `22320` of `327680` bytes, Flash `398994` of `1310720` bytes |
| `python -m platformio run -e esp32s2dev` | `esp32s2dev` success in `00:00:14.952`; RAM `36752` of `327680` bytes, Flash `390933` of `1310720` bytes |
| `python -m platformio pkg pack` | Exit code 0; wrote `C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\ADS1115-1.0.0.tar.gz` |
| Package artifact cleanup | `Removed ADS1115-1.0.0.tar.gz` |
| `idf.py` availability/build command | `idf.py unavailable`; local pure ESP-IDF builds were not run |

## CI Coverage

`.github/workflows/ci.yml` configures:

- Native PlatformIO tests.
- Core timing/framework guard.
- CLI contract guard.
- ESP-IDF example contract guard.
- Version metadata check.
- Arduino ESP32-S3 PlatformIO build.
- Arduino ESP32-S2 PlatformIO build.
- Pure ESP-IDF example container builds for ESP32-S3 and ESP32-S2.
- PlatformIO package packing.

The workflow triggers on pushes to `main`, pushes to `hardening/**`, pull
requests targeting `main`, and manual `workflow_dispatch` runs. This branch
currently has local command evidence and CI configuration evidence; a CI run URL
should be recorded before claiming branch CI execution evidence.

Guard limitation: CLI and ESP-IDF contract guards are token/smoke checks, not
full behavioral integration tests.

Package limitation: CI runs `pio pkg pack`, but does not yet inspect archive
contents against an allow/deny export policy.

## Hardware Validation Status

Hardware validation has not been executed in this repository state. No dated HIL
logs, oscilloscope captures, logic-analyzer traces, soak logs, or fault-injection
transcripts are present.

Prepared artifacts:

- `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md`
- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`
- `tools/hil_ads1115_capture.py`

The hardware validation plan covers branch/commit/version identity, ADS1115
module and electrical setup, address straps, wrong/missing address behavior,
MUX selections, PGA ranges with safe inputs, all data rates, single-shot and
continuous modes, blocking and service/tick paths, comparator modes/latch/
polarity/queue, ALERT/RDY captures, stuck bus/unplug/replug/brownout/recover,
partial write/fault injection if possible, Arduino S2/S3 CLI, ESP-IDF S2/S3
example, 24-hour nominal soak, and 2-hour 860 SPS stress.

## Remaining Blockers

Must fix before merge:

- Rebase or merge with current `origin/main`, resolve conflicts reported by
  `git merge-tree --write-tree HEAD origin/main`, and rerun validation.
- Obtain a PR CI run or otherwise record branch CI execution evidence if CI
  status is part of the merge gate.

Must validate before release:

- Execute the HIL validation plan and attach dated logs/captures.
- Capture ALERT/RDY timing evidence at 8, 128, and 860 SPS.
- Validate comparator traditional/window, latch, polarity, and queue behavior.
- Validate stuck bus, unplug/replug, brownout/reset, recover, and partial write
  or fault-injection cases.
- Run and record pure ESP-IDF builds in CI or a local ESP-IDF environment.
- Decide and apply release versioning. Do not release this hardening work as
  another `1.0.0`.

Future industry-grade evidence:

- PR/run URLs for CI jobs on the final rebased branch.
- Package archive contents allow/deny guard.
- Behavioral integration tests for CLI and ESP-IDF examples beyond token guards.
- Multiple module/board/pull-up/bus-speed validation matrix runs.
- Environmental and long-soak logs across realistic operating conditions.

Nice-to-have:

- Broaden package validation to inspect `library.json` export behavior.
- Add automated parsing for selected HIL transcripts while keeping raw logs.
- Add a README pointer that the Prompt 08 final report supersedes older
  historical hardening reports.

## Release Wording Recommendation

Acceptable wording if the final rebased branch passes checks but HIL remains
incomplete:

> Production-oriented ADS1115 driver with framework-neutral core, injected I2C
> transport, explicit timing/error contracts, strong native fault tests, Arduino
> ESP32-S2/S3 build coverage, and prepared ESP-IDF/HIL validation paths.
> Hardware/fault validation remains required before field-grade claims.

Do not claim:

- field-proven
- fully industry-grade
- production-ready
- all hardware validated
- hardware validated

unless the repository contains actual dated evidence supporting those claims.

## Merge Recommendation

Not ready to merge as-is into current `origin/main`.

Ready with conditions after:

- Conflict resolution against current `origin/main`.
- Full validation rerun on the resolved branch.
- PR CI or equivalent branch CI evidence, if required by the project gate.

## Release Recommendation

Ready as pre-release/API-stable candidate only after merge conflicts are resolved
and checks pass.

Not ready for production or field-grade release claims until hardware validation
evidence is complete.

Recommended version strategy: at least `1.1.0` for backward-compatible public
API/status additions. Consider `2.0.0` only if exact status-behavior changes are
treated as breaking for downstream consumers.

## Future Work Backlog

- Rebase or merge current `origin/main` into this branch and resolve conflicts.
- Rerun all Prompt 08 validation after conflict resolution.
- Open PR and record CI run URLs.
- Execute HIL validation plan and fill the results template with dated evidence.
- Add package archive contents validation.
- Strengthen CLI/IDF guards from token checks toward behavioral tests.
- Decide release version and update `library.json`, `idf_component.yml`,
  `Doxyfile`, generated `Version.h`, README, and CHANGELOG consistently.

## Files Changed

Relative to `origin/main...HEAD` before this final report, the hardening branch
changed:

- `.github/workflows/ci.yml`
- `AGENTS.md`
- `CHANGELOG.md`
- `CMakeLists.txt`
- `README.md`
- `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md`
- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`
- `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md`
- `docs/ADS1115_SELFTEST_POLISH_REPORT.md`
- `docs/CODEX_PROMPT_ADS1115_DRIVER.md`
- `docs/HARDENING_FINAL_REPORT.md`
- `docs/IDF_PORT.md`
- `docs/extracted-md/02_pinout_and_signals.md`
- `docs/extracted-md/06_modes_interrupts_status_and_faults.md`
- `examples/01_basic_bringup_cli/main.cpp`
- `examples/common/HealthDiag.h`
- `examples/common/I2cScanner.h`
- `examples/common/I2cTransport.h`
- `examples/esp_idf/basic/CMakeLists.txt`
- `examples/esp_idf/basic/main/CMakeLists.txt`
- `examples/esp_idf/basic/main/main.cpp`
- `idf_component.yml`
- `include/ADS1115/ADS1115.h`
- `include/ADS1115/Config.h`
- `include/ADS1115/Status.h`
- `library.json`
- `scripts/generate_version.py`
- `src/ADS1115.cpp`
- `test/test_basic.cpp`
- `tools/check_cli_contract.py`
- `tools/check_core_timing_guard.py`
- `tools/check_idf_example_contract.py`
- `tools/hil_ads1115_capture.py`

Prompt 08 additionally changed:

- `docs/ADS1115_INDUSTRY_STANDARD_FINAL_REPORT.md`
- `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md`
- `README.md`
- `CHANGELOG.md`
- `src/ADS1115.cpp`
