# ADS1115 Industry-Standard Implementation Report

Branch: `hardening/ads1115-industry-standard-p0`
Starting commit: `65f6fdcc5c7b1d4da2d94eec5e4393614598e3f7`
Source audit report: `exploration/ads1115-industry-standard:docs/ADS1115_INDUSTRY_STANDARD_EXPLORATION.md` at `d6f163534d59ad8bdd4b597bfd7f64a93615ac94`
Status: P0 contract tests are being added test-first. No production
implementation fixes are marked complete yet.

## Scope Rules

- Proceed one bounded prompt at a time; no broad redesign.
- Preserve framework-neutral core code in `include/` and `src/`.
- Keep I2C transport injected and non-owning.
- Do not introduce Arduino, Wire, ESP-IDF, FreeRTOS, logging, global bus, pin
  ownership, task, or framework delay dependencies into the core.
- Public fallible production APIs must return meaningful `Status`.
- Append status enum values unless an explicit compatibility note justifies
  reordering.
- ADS1115 strict init/read-back is plausibility/read-back only; the device has no
  chip-ID register.
- Dirty/partial hardware-state diagnostics must be explicit for multi-register
  writes and raw diagnostic writes.
- Public APIs are not ISR-safe and instances are not internally thread-safe
  unless proven, documented, and tested.
- Hardware validation claims require dated logs or captures.
- CI/build claims require command output or CI configuration evidence.
- Every prompt ends with commit and push/sync.

## Chunk Plan

| Prompt | Planned scope | Implementation status |
| --- | --- | --- |
| 01 | Branch baseline, rules, and tracking report | Docs/rules only |
| 02 | P0 native test design for status and partial hardware state | Contract tests added; implementation pending |
| 03 | P0 core status taxonomy and begin dirty-state implementation | Not started |
| 04 | P0 raw register cache/dirty contract | Not started |
| 05 | P1 API contracts for `tick()`, `nowMs`, and blocking latency | Not started |
| 06 | P1 tests, guards, and documentation polish | Not started |
| 07 | Integration examples, CI evidence, and HIL validation matrix | Not started |
| 08 | Final report and release-readiness review | Not started |

## Source Audit Summary

The exploration report classified the library as near industry-grade pending P0
fixes and validation. The highest-priority follow-up work is:

- Preserve visibility of partial hardware mutation when `begin()` fails after one
  or more writes.
- Tighten status taxonomy for offline, unsupported operation, strict read-back
  mismatch, and dirty/partial hardware state.
- Resolve or document public paths that can hide transport failure, especially
  bool-only readiness and void service paths.
- Define raw diagnostic register write cache/dirty semantics.
- Add native fault-injection coverage for begin-path partial writes, strict
  read-back branches, raw scaling, invalid config, and related status contracts.
- Produce dated HIL logs/captures before making field-grade hardware validation
  claims.

## Prompt Results

| Prompt | Branch/commit | Files changed | Validation | Result |
| --- | --- | --- | --- | --- |
| 01 | `38e55ea127a33aeb9fa601ce3d74b6e21856fe70` | `AGENTS.md`, `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` | Guards passed; native PlatformIO passed | Rules/report initialization only; no core implementation complete |
| 02 | Pending commit | `include/ADS1115/Status.h`, `test/test_basic.cpp`, `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` | Core guard passed; native PlatformIO has expected contract failures | Test-first contracts added; production implementation pending |

## Prompt 02 Contract Tests

Status taxonomy additions in `include/ADS1115/Status.h` are append-only:
`OFFLINE`, `UNSUPPORTED_OPERATION`, `READBACK_MISMATCH`, and
`HARDWARE_CONFIG_DIRTY`.

Added or updated native tests:

| Test | Expected behavior | Current status |
| --- | --- | --- |
| `test_status_taxonomy_additions_are_append_only` | New `Err` values append after existing `I2C_BUS` values. | Pass |
| `test_begin_strict_readback_mismatch_fails_without_initializing_and_preserves_dirty` | Strict read-back mismatch returns `READBACK_MISMATCH`, leaves driver uninitialized, and preserves dirty diagnostic. | Fails: returns `I2C_ERROR` |
| `test_begin_failure_after_first_apply_write_preserves_dirty_diagnostic` | Failed `begin()` after `LO_THRESH` reached hardware preserves dirty diagnostic and original transport status. | Fails: dirty diagnostic is cleared |
| `test_begin_failure_after_second_apply_write_preserves_dirty_diagnostic` | Failed `begin()` after `LO_THRESH` and `HI_THRESH` reached hardware preserves dirty diagnostic and original transport status. | Fails: dirty diagnostic is cleared |
| `test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic` | Strict read transport failure after all writes preserves original transport status and dirty diagnostic. | Fails: dirty diagnostic is cleared |
| `test_successful_begin_clears_prior_failed_begin_dirty_diagnostic` | Later successful `begin()` clears retained failed-begin dirty diagnostic only after full resync succeeds. | Fails at retained dirty diagnostic precondition |
| `test_recover_strict_readback_mismatch_keeps_dirty_and_preserves_error` | Strict read-back mismatch during recover returns and stores `READBACK_MISMATCH`. | Fails: returns `I2C_ERROR` |
| `test_offline_latches_normal_read_returns_offline_without_i2c_until_recover` | Public tracked I2C while offline returns `OFFLINE` without bus access. | Fails: returns `BUSY` |
| `test_failed_recover_from_offline_preserves_latch_after_partial_success` | Offline latch remains explicit as `OFFLINE` after failed recover. | Fails: returns `BUSY` |
| `test_start_conversion_in_continuous_mode_returns_unsupported_operation` | `startConversion()` overloads in continuous mode return `UNSUPPORTED_OPERATION` without bus access. | Fails: returns `BUSY` |
| `test_raw_config_write_marks_hardware_config_dirty_without_cache_commit` | Successful raw CONFIG write is diagnostic, updates hardware, leaves typed cache unchanged, and marks dirty/stale with `HARDWARE_CONFIG_DIRTY`. | Fails: dirty diagnostic is not set |
| `test_raw_low_threshold_write_marks_hardware_config_dirty_without_cache_commit` | Successful raw low-threshold write marks dirty/stale without typed cache commit. | Fails: dirty diagnostic is not set |
| `test_raw_high_threshold_write_marks_hardware_config_dirty_without_cache_commit` | Successful raw high-threshold write marks dirty/stale without typed cache commit. | Fails: dirty diagnostic is not set |
| Existing invalid raw register and conversion-register write tests | Invalid register remains rejected without bus access; conversion register remains read-only. | Pass |

Implementation work required next:

- Preserve failed-`begin()` dirty diagnostics across the uninitialized state while
  retaining the original transport or read-back error.
- Return `READBACK_MISMATCH` for strict low/high/config read-back mismatches.
- Return `OFFLINE` from offline tracked I2C short-circuits.
- Return `UNSUPPORTED_OPERATION` for continuous-mode single-shot start requests.
- Mark successful raw CONFIG/threshold diagnostic writes as cache/hardware dirty
  or stale using `HARDWARE_CONFIG_DIRTY`, while keeping typed cache unchanged.

## Validation Log

Prompt 01 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h` |
| `python -m platformio test -e native` | `57 test cases: 57 succeeded in 00:00:02.147`; environment `native`, status `PASSED` |

Prompt 02 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python -m platformio test -e native` | `ERRORED`; `67 test cases: 13 failed, 53 succeeded in 00:00:02.286` |

Expected Prompt 02 native failures:

- `test_begin_strict_readback_mismatch_fails_without_initializing_and_preserves_dirty`: expected `READBACK_MISMATCH`, got `I2C_ERROR`.
- `test_begin_failure_after_first_apply_write_preserves_dirty_diagnostic`: expected dirty diagnostic, got clean state.
- `test_begin_failure_after_second_apply_write_preserves_dirty_diagnostic`: expected dirty diagnostic, got clean state.
- `test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic`: expected dirty diagnostic, got clean state.
- `test_successful_begin_clears_prior_failed_begin_dirty_diagnostic`: expected retained dirty diagnostic before retry, got clean state.
- `test_recover_strict_readback_mismatch_keeps_dirty_and_preserves_error`: expected `READBACK_MISMATCH`, got `I2C_ERROR`.
- `test_offline_latches_normal_read_returns_offline_without_i2c_until_recover`: expected `OFFLINE`, got `BUSY`.
- `test_failed_recover_from_offline_preserves_latch_after_partial_success`: expected `OFFLINE`, got `BUSY`.
- `test_start_conversion_in_continuous_mode_returns_unsupported_operation`: expected `UNSUPPORTED_OPERATION`, got `BUSY`.
- `test_raw_config_write_marks_hardware_config_dirty_without_cache_commit`: expected dirty diagnostic, got clean state.
- `test_raw_low_threshold_write_marks_hardware_config_dirty_without_cache_commit`: expected dirty diagnostic, got clean state.
- `test_raw_high_threshold_write_marks_hardware_config_dirty_without_cache_commit`: expected dirty diagnostic, got clean state.
- `test_shutdown_offline_returns_offline_without_bus_access`: expected `OFFLINE`, got `BUSY`.

## Final Report Placeholder

Final release-readiness conclusions will be written after all planned hardening
chunks complete and after the required validation evidence is available.
