# ADS1115 1.1.0 Release Notes

Release date: 2026-06-02
Release type: minor
Recommended tag: `v1.1.0`

## Release Summary

ADS1115 `1.1.0` is a production-oriented/API-stable release candidate with a
framework-neutral core, injected non-owning I2C transport, explicit status
taxonomy, partial-hardware-state diagnostics, stronger native fault coverage,
Arduino ESP32-S2/S3 build coverage, native ESP-IDF example contract coverage,
and limited COM19 HIL evidence.

Do not describe this release as field-proven, production-ready, fully
industry-grade, or fully hardware validated. The current hardware evidence is
limited and scoped.

## Notable Changes

- Added status codes without reordering existing enum values:
  `OFFLINE`, `UNSUPPORTED_OPERATION`, `READBACK_MISMATCH`, and
  `HARDWARE_CONFIG_DIRTY`.
- Added failed-`begin()` dirty diagnostics so applications can see when hardware
  may have been partially configured even though initialization failed.
- Added raw-register dirty-state behavior: successful raw writes to writable
  registers leave typed cache unchanged and mark hardware/cache sync dirty until
  a full verified resync.
- Added status-returning readiness/service APIs while preserving compatibility
  helpers.
- Clarified no-clock timing behavior, blocking-read bounds, and continuous-mode
  latest-register semantics.
- Added native ESP-IDF example/adapter structure and guards.
- Expanded native tests and guard scripts for framework leakage, status
  taxonomy, strict read-back, rollback, scaling, and timing boundaries.
- Archived historical hardening reports under `docs/archive/` and added
  `docs/README.md` as the current documentation map.

## Validation Summary

The final release-prep validation should include:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

Remove the generated `ADS1115-1.1.0.tar.gz` after package validation unless it
is intentionally attached to the GitHub release.

## Limited HIL Evidence

Tracked evidence:

- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_2026-06-02_COM19.md`
- `docs/evidence/hil/2026-06-02_COM19/ads1115_hil_20260602_205201.log`

Evidence scope:

- `0x48` present/pass.
- `0x49` present/pass.
- `0x4A` absent/pass-as-negative-test.
- `0x4B` absent/pass-as-negative-test.
- Initialized-address selftest reported `pass=29 fail=0 skip=0`.
- `stress 500` and `stress 1000` completed with zero errors.
- `stress_mix 200` completed with `ok=200 fail=0`.
- Final driver state was `READY` and online.

The HIL run was captured before the final `1.1.0` metadata bump. It is limited
hardware evidence for the exercised behavior, not full release hardware
validation.

## Remaining Hardware Gaps

- No ALERT/RDY pulse capture.
- No comparator electrical validation.
- No full mux/gain/rate sweep with measured input sources.
- No stuck bus, unplug/replug, brownout/reset, or partial-write injection.
- No long soak.
- No pure ESP-IDF hardware run.
- Incomplete physical setup metadata for the COM19 run.

## Release Wording

Acceptable:

> Production-oriented ADS1115 driver with framework-neutral core, injected I2C
> transport, explicit timing/error contracts, strong native fault tests, Arduino
> ESP32-S2/S3 build coverage, native ESP-IDF example contract coverage, and
> limited COM19 HIL evidence for address handling, restore sequencing,
> selftests, and short stress runs.

Avoid:

- field-proven
- production-ready
- fully industry-grade
- fully hardware validated
- all hardware validated
