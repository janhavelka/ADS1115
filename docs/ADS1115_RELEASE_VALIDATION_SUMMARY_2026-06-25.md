# ADS1115 Release Validation Summary - 2026-06-25

This is the compact record retained after cleaning prompt-era HIL reports and
local ignored HIL logs. It is a release-readiness summary, not a raw evidence
archive.

## Current Release Status

- Current claim: production-oriented/API-stable release candidate.
- Not claimed: production-grade hardware validation.
- Reason: clean release-candidate HIL, calibrated analog fixture evidence,
  ALERT/RDY electrical captures, physical fault matrix, and pure ESP-IDF
  hardware validation are still incomplete.

## Local Host Validation

The current work added and exercised:

- `Err::CLOCK_STALLED` and deterministic stalled-clock reporting.
- Fresh-sample `readBlocking()` behavior in continuous mode.
- Continuous-mode staged config apply.
- Precise dirty diagnostics for first single-shot staged write failures.
- Dirty hardware address preservation in `SettingsSnapshot`.
- Compatibility-only `conversionReady()` documentation and example migration to
  `readConversionReady(bool&)`.
- ESP-IDF diagnostic CLI address selection and dirty/timebase settings parity.
- HIL runner contract/evidence verdict split, `EVIDENCE_REQUIRED` rows,
  `--fail-on-unknown` / `--fail-on-evidence-required`, serial-exception
  classification, and summary-anchored `stress_mix` parsing.

Release commit validation on 2026-06-25:

- `python tools\run_i2c_hil.py --parser-test`: PASS.
- `python tools\run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted`: PASS.
- `python tools\check_cli_contract.py`: PASS.
- `python tools\check_idf_example_contract.py`: PASS.
- `python scripts\generate_version.py check`: PASS.
- `python tools\check_core_timing_guard.py`: PASS.
- `pio test -e native`: PASS, `151/151`.
- `pio run -e esp32s2dev`: PASS.
- `pio run -e esp32s3dev`: PASS.
- `pio pkg pack`: PASS, generated archive removed after the check.
- `git diff --check`: PASS.
- `idf.py --version`: NOT RUN, `idf.py` was not on PATH.

## COM8 Targeted HIL - 2026-06-23

- Board: ESP32-S2 on `COM8`.
- Devices: ADS1115-range addresses `0x48` and `0x49`.
- Firmware reported: `ADS1115 library full: 1.1.0 (8476da2, 2026-06-23 11:35:29, dirty)`.
- Command: `python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite targeted --timeout-s 8 --idle-s 0.5 --boot-settle-s 2 --stop-on-fail`.
- Result: `PASS=154`, `FAIL=0`, evidence-required rows `24`.
- Contract verdict: `PASS`.
- Evidence verdict: `EVIDENCE_REQUIRED`.

Coverage included two-address selection, probe/settings/health/recover, data
rates, gains, mux paths, single-shot and continuous modes, comparator controls,
register and dirty-state diagnostics, staged jobs with budgets `0`, `1`, and
`255`, active-job `BUSY` interleaving, short stress, and malformed input
rejection.

This validates the flashed dirty firmware only. It is not clean release-candidate
evidence.

## COM8 Functional And 8-Hour HIL - 2026-06-22/23

- Board: ESP32-S2 on `COM8`.
- Devices: ADS1115-range addresses `0x48` and `0x49`.
- Runner summary commit: `bdd36501b129cb2309a8fbeae4da24d04618f99e`.
- Firmware reported dirty at runtime.
- Smoke gate: `PASS=12`, `FAIL=0`, evidence-required rows `2`.
- Full functional plus benchmark suite: `PASS=188`, `FAIL=0`,
  evidence-required rows `42`.
- 8-hour soak summary row: `PASS`.
- 8-hour soak duration: `28799.4 s`.
- 8-hour soak cycles: `16296`.
- 8-hour soak commands: `717010`.
- 8-hour soak classified results: `PASS=554051`,
  evidence-required rows `162959`, `FAIL=0`.
- Worst 8-hour soak command latency: `0.485 s`.
- Mean 8-hour soak command latency: `0.039 s`.

This older HIL pass covered the broad CLI contract, benchmarks, and an 8-hour
mixed command soak. The raw ignored transcripts and small tracked summary files
were removed during cleanup; these compact metrics are retained here.

## COM8 Intensive Soak - 2026-06-24/25

- Board: ESP32-S2 on `COM8`.
- Devices: ADS1115-range addresses `0x48` and `0x49`.
- Firmware reported: `ADS1115 library full: 1.1.0 (8476da2, 2026-06-23 11:35:29, dirty)`.
- Command plan: `--suite exhaustive --benchmark`.
- Soak duration requested: `72000` seconds.
- Observed duration: `71998.6 s`.
- Cycles: `40693`.
- Soak commands: `1790466`.
- Soak classified results: `PASS=1383541`, `EVIDENCE_REQUIRED=406925`, `FAIL=0`.
- Command-plan rows: `PASS=189`, `FAIL=0`, `EVIDENCE_REQUIRED=42`.
- Contract verdict: `PASS`.
- Evidence verdict: `EVIDENCE_REQUIRED`.
- Worst command latency: `0.406 s`.
- Mean command latency: `0.039 s`.

The deleted local ignored artifacts had:

- Transcript path: `hil_logs\ads1115_20h_soak_20260624_045300_retry2\ads1115_hil_20260624_045300.log`.
- Transcript bytes: `595223364`.
- Transcript SHA-256: `1187EEE410B0700FF5CE6F5888924B877258920B7951F580A9ACC536B894B622`.
- Summary path: `hil_logs\ads1115_20h_soak_20260624_045300_retry2\ads1115_hil_20260624_045300_summary.md`.
- Summary bytes: `54706`.
- Summary SHA-256: `E50C30617F10F5AF88802B22E21C2F00CBE224A8AB34487FE6723F496816C9D7`.

An earlier 20-hour attempt ran about 14.5 hours with `FAIL=0` at inspection, but
the host process stopped on a pySerial `SerialException`. The runner now converts
serial command write/read exceptions into classified failure rows instead of
uncaught tracebacks.

This is strong digital/API stability evidence for the flashed dirty firmware. It
is not clean release-candidate evidence and does not prove analog accuracy or
ALERT/RDY electrical behavior.

## Remaining Evidence Gates

Before making a production-grade claim, complete and archive:

- Clean release-candidate targeted, full, and soak HIL from `git diff --exit-code`
  firmware whose `version` output matches the tagged commit and is not dirty.
- Calibrated analog fixture results for all required MUX, gain, and data-rate
  families.
- ALERT/RDY and comparator electrical captures for conversion-ready, traditional
  comparator, window comparator, polarity, latch, and queue-depth behavior.
- Physical fault matrix: missing device, unplug/replug, stuck SDA/SCL, brownout,
  raw-write dirty visibility, and recovery.
- Pure ESP-IDF hardware validation on ESP32-S2 and ESP32-S3.
- Immutable raw evidence manifests with hashes and links under `docs/evidence/`.
