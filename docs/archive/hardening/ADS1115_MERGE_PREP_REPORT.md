# ADS1115 Merge-Prep Report

> Historical merge-preparation record. Its remaining-gap language applies to
> the dated branch, not to the current release. See
> [`../../OPEN_ITEMS.md`](../../OPEN_ITEMS.md) for unfinished work.

Date: 2026-06-02
Branch: `hardening/ads1115-industry-standard-p0`
Resolved merge commit: `fc262d934b68eff54b08e598ace31b06a69f0998`
Merged base: `origin/main`

## Summary

The hardening branch has been merged with current `origin/main`, conflicts were
resolved, and the full requested local validation set passes. The corrected
COM19 HIL run is now tracked as limited hardware evidence. It supports address
handling, restore sequencing, initialized-address selftests, and short stress
runs only; it does not complete full hardware validation.

## Conflict Resolution Summary

Resolved conflicts in:

- `.github/workflows/ci.yml`: kept the stricter hardening CI workflow, including
  hardening branch pushes, version checks, ESP-IDF example contract checks,
  ESP-IDF container builds, and package validation.
- `AGENTS.md`: kept hardening rules for thread-safety, ISR safety,
  partial-state diagnostics, and transport ownership; retained the `origin/main`
  ESP-IDF adapter ownership note.
- `CHANGELOG.md`: kept precise `Err::OFFLINE` wording and added native ESP-IDF
  CLI coverage notes from `origin/main`.
- `CMakeLists.txt`: kept the `origin/main` IDF component CMake with generated
  version header support.
- `README.md`: kept conservative production-oriented wording and added native
  ESP-IDF CLI coverage plus the limited COM19 HIL evidence pointer.
- `docs/IDF_PORT.md`: kept the newer hardening portability/error-mapping
  limitation table and added a note for the merged full native ESP-IDF CLI.
- `examples/01_basic_bringup_cli/main.cpp`: kept the hardening address-selection
  state model and updated it to use the merged Arduino transport `configUser()`
  helper.
- `examples/common/I2cTransport.h`: removed duplicate merged `arduinoNowMs()` and
  `arduinoYield()` definitions; kept `configUser()`.
- `examples/esp_idf/basic/**` and `tools/check_idf_example_contract.py`: kept the
  fuller `origin/main` native ESP-IDF CLI and split transport implementation.
- `include/ADS1115/Config.h`: kept hardening API documentation and
  `strictInitVerify`.
- `idf_component.yml`: kept framework-neutral description and shared ESP-IDF
  target/dependency metadata.

## Validation Results

All commands below were run after conflict resolution from the final resolved
tree.

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | Exit 0; `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | Exit 0; `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | Exit 0; `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | Exit 0; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | Exit 0; `112 test cases: 112 succeeded in 00:00:02.201` |
| `python -m platformio run -e esp32s3dev` | Exit 0; `esp32s3dev` success in `00:00:14.602`; RAM `22320` of `327680`; Flash `399950` of `1310720` |
| `python -m platformio run -e esp32s2dev` | Exit 0; `esp32s2dev` success in `00:00:13.255`; RAM `36768` of `327680`; Flash `391905` of `1310720` |
| `python -m platformio pkg pack` | Exit 0; wrote `ADS1115-1.0.0.tar.gz` |
| `Remove-Item -LiteralPath .\ADS1115-1.0.0.tar.gz` | Exit 0; generated package artifact removed |

An earlier post-merge `esp32s3dev` build failed because
`examples/common/I2cTransport.h` had duplicate merged callback definitions.
That conflict artifact was fixed, and the final validation above passed.

## CI Status

No CI run URL was available at report creation time. The merged workflow is
configured to run on `main`, `hardening/**` pushes, pull requests targeting
`main`, and manual dispatch. Record the GitHub Actions run URL after this report
is pushed and CI completes.

## Corrected HIL Evidence

Tracked raw transcript:

- `docs/evidence/hil/2026-06-02_COM19/ads1115_hil_20260602_205201.log`

Results report:

- `docs/evidence/hil/2026-06-02_COM19/README.md`

Limited COM19 evidence summary:

- `0x48`: present/pass.
- `0x49`: present/pass.
- `0x4A`: absent/pass-as-negative-test.
- `0x4B`: absent/pass-as-negative-test.
- `selftest`: `pass=29 fail=0 skip=0` on initialized addresses.
- `stress 500`: `500` success, `0` errors.
- `stress 1000`: `1000` success, `0` errors.
- `stress_mix 200`: `ok=200 fail=0`.
- Final driver health: `READY`, online `yes`, total failures `0`, last error
  `never`.

Transport precision note: absent `0x4A` and `0x4B` returned generic
`I2C_ERROR` because Arduino/ESP32 `Wire.requestFrom()` exposed a zero-byte read
without reliable read-phase NACK detail. This was not globally remapped to
`DEVICE_NOT_FOUND`; it is documented as a diagnostic Arduino transport precision
limitation.

## Remaining Hardware Gaps

The current HIL evidence does not cover:

- ALERT/RDY pulse capture.
- Comparator electrical validation.
- Full mux/gain/rate sweep with measured input sources.
- Stuck bus, unplug/replug, brownout/reset, or partial-write fault injection.
- Long soak.
- Pure ESP-IDF hardware run.
- Full operator/electrical metadata such as VDD, pull-ups, instruments, wiring
  photos, and ambient conditions.

## Version Recommendation

Version metadata was not bumped in this merge-prep step. Do not publish this
hardening work as another `1.0.0`. Recommended release version is `1.1.0` for
backward-compatible API/status additions unless project policy classifies the
status behavior changes as breaking.

## Merge Verdict

Ready to open or update a PR and run CI. Ready to merge after the pushed branch
CI/PR checks pass and the CI URL is recorded.

## Release Verdict

Not ready for production or field-grade release claims. Ready only as a
production-oriented/API-stable candidate after CI passes and version metadata is
bumped consistently. Full release claims require the remaining hardware
validation evidence listed above.
