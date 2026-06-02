# ADS1115 Industry-Standard Implementation Report

Branch: `hardening/ads1115-industry-standard-p0`
Starting commit: `65f6fdcc5c7b1d4da2d94eec5e4393614598e3f7`
Source audit report: `exploration/ads1115-industry-standard:docs/ADS1115_INDUSTRY_STANDARD_EXPLORATION.md` at `d6f163534d59ad8bdd4b597bfd7f64a93615ac94`
Status: P0 status taxonomy, failed-begin dirty diagnostics, strict read-back
mismatch status, raw diagnostic write dirty marking, P1 readiness/timing API
contract polish, and P1 guard/test/documentation expansion are implemented and
validated in native tests. Integration examples and HIL validation artifacts are
prepared. Hardware validation evidence is still pending.

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
| 02 | P0 native test design for status and partial hardware state | Contract tests added |
| 03 | P0 core status taxonomy and begin dirty-state implementation | Implemented |
| 04 | P0 raw register cache/dirty contract | Implemented; recovery closure tests/docs added |
| 05 | P1 API contracts for `tick()`, `nowMs`, and blocking latency | Implemented |
| 06 | P1 tests, guards, and documentation polish | Implemented |
| 07 | Integration examples, CI evidence, and HIL validation matrix | Implemented; hardware execution pending |
| 08 | Final report and release-readiness review | Implemented; merge/release gates documented |

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
| 02 | `7ce66d14cef39e5f2c6a7b497bcf02c65770c764` | `include/ADS1115/Status.h`, `test/test_basic.cpp`, `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` | Core guard passed; native PlatformIO had expected contract failures | Test-first contracts added |
| 03 | `effddc526ab51927488acb12ede1e6586cde00c0` | `src/ADS1115.cpp`, `include/ADS1115/ADS1115.h`, `test/test_basic.cpp`, `README.md`, `examples/01_basic_bringup_cli/main.cpp`, `examples/common/HealthDiag.h`, `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` | Required guards, native tests, and Arduino builds passed | P0 status and dirty diagnostics implemented |
| 04 | `8056c962a010b4db83551d2c4805514899886ee5` | `src/ADS1115.cpp`, `include/ADS1115/ADS1115.h`, `test/test_basic.cpp`, `README.md`, `examples/01_basic_bringup_cli/main.cpp`, `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` | Core/CLI guards, native tests, and Arduino builds passed | Raw diagnostic write cache/dirty contract closed with recovery tests and docs |
| 05 | `3f4583b0138d4cf9694ceaeeb35e1ffa164db24b` | `src/ADS1115.cpp`, `include/ADS1115/ADS1115.h`, `include/ADS1115/Config.h`, `test/test_basic.cpp`, `README.md`, `docs`, `examples/01_basic_bringup_cli/main.cpp`, `tools/check_core_timing_guard.py` | Required guards, native tests, and Arduino builds passed | Readiness/status aliases, service timing, no-clock diagnostics, and blocking bounds clarified |
| 06 | `73f87fee663473340a7d53428ab1f6bc113067bd` | `test/test_basic.cpp`, `tools/check_core_timing_guard.py`, `scripts/generate_version.py`, `README.md`, `CHANGELOG.md`, `docs`, `include/ADS1115/ADS1115.h` | Required guards, version check, native tests, Arduino builds, and package pack passed | Edge coverage, core leakage guards, version metadata sync, and documentation honesty expanded |
| 07 | `c4558b293a66975112a0db85c10a2cd8dde814b4`; current recovery evidence through this Prompt 07 re-verification commit | `examples`, `.github`, `tools`, `README.md`, `docs` | Required local guards/tests/builds passed; `idf.py` local availability recorded; current native evidence is 112 tests | Integration examples clarified, CI evidence strengthened, ESP-IDF mapping limits documented, and HIL operator plan/template/script added/re-verified |
| 08 | Report commit containing `docs/ADS1115_INDUSTRY_STANDARD_FINAL_REPORT.md` | `docs/ADS1115_INDUSTRY_STANDARD_FINAL_REPORT.md`, this report, `README.md`, `CHANGELOG.md`, `src/ADS1115.cpp` | Required local guards/tests/builds/package pack passed; `idf.py` unavailable; read-only merge-tree reports conflicts with current `origin/main` | Final readiness report added; branch requires rebase/merge conflict resolution before merge and dated HIL evidence before release claims |

## Prompt 02 Contract Tests

Status taxonomy additions in `include/ADS1115/Status.h` are append-only:
`OFFLINE`, `UNSUPPORTED_OPERATION`, `READBACK_MISMATCH`, and
`HARDWARE_CONFIG_DIRTY`.

Added or updated native tests. This table records the intentional test-first
state from Prompt 02; the listed failing contracts were implemented in later
prompts and all pass in the current 112-case native validation.

| Test | Expected behavior | Prompt 02 historical status |
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

Prompt 02 crash-recovery verification on 2026-06-02:

- `status-taxonomy-agent`: confirmed `OFFLINE`, `UNSUPPORTED_OPERATION`,
  `READBACK_MISMATCH`, and `HARDWARE_CONFIG_DIRTY` are appended after
  `I2C_BUS`, the native enum-order test pins their numeric values, and current
  tests cover offline short-circuit, unsupported continuous-mode conversion
  starts, strict read-back mismatch, structural dirty diagnostics, and raw
  diagnostic write dirty behavior.
- `begin-partial-state-test-agent`: confirmed failed-`begin()` tests cover
  partial `_applyConfig()` writes, uninitialized diagnostic visibility, strict
  read-back transport failures, successful retry clearing after full resync, and
  failed retry preserving the dirty diagnostic. It noted that the third
  `_applyConfig()` write failure was covered by behavior but not by a dedicated
  test name, and that default threshold values made partial-write assertions
  less explicit.
- `strict-readback-test-agent`: confirmed config/threshold strict read-back
  mismatch tests and strict transport-failure tests are present. It initially
  noted that config mismatch tests did not assert returned `Status::detail`;
  the Prompt 02 recovery test tightening below closed that gap.
- `raw-register-contract-agent`: confirmed Option A is recorded and tested:
  raw CONFIG/threshold writes are diagnostic writes that update hardware, leave
  typed cache unchanged, and mark `HARDWARE_CONFIG_DIRTY`; invalid raw registers
  and conversion-register writes remain rejected before bus access.
- `compatibility-review-agent`: confirmed Prompt 02 commit
  `7ce66d14cef39e5f2c6a7b497bcf02c65770c764` changed only the report,
  `include/ADS1115/Status.h`, and `test/test_basic.cpp`; production
  implementation was not overreached, `Status` layout was preserved, and enum
  additions were append-only.

Prompt 02 recovery test tightening:

- Added
  `test_begin_failure_on_third_apply_write_preserves_original_status_and_dirty`
  to explicitly name and pin failure of the third `_applyConfig()` transaction
  during `begin()`: original transport status/detail are returned, the driver
  remains uninitialized, prior threshold writes are visible in fake hardware,
  CONFIG remains unwritten, and the dirty diagnostic remains structural.
- Strengthened first/second partial-`begin()` write tests with non-default
  threshold/config values so fake hardware assertions prove which writes reached
  hardware before the injected failure.
- Strengthened config strict-read-back mismatch tests for `begin()` and
  `recover()` to assert returned `Status::detail` carries the observed raw
  register value, matching the existing low/high threshold detail contract.

Current implementation work required next for Prompt 02 coverage: none. The
historical test-first failures listed below were implemented by later prompts,
and the recovery-tightened contracts now pass on the current branch.

Prompt 02 re-verification on 2026-06-02 at current tip after Prompt 03 recovery:

- `status-taxonomy-agent`: found no blocking taxonomy gaps. Append-only numeric
  order remains pinned, and current tests cover offline no-bus-touch,
  continuous-mode `UNSUPPORTED_OPERATION`, strict `READBACK_MISMATCH`, and
  structural partial-state diagnostics.
- `begin-partial-state-test-agent`: confirmed first/second/third begin partial
  write failures, original status/detail preservation, uninitialized diagnostic
  visibility, strict read-back transport failures for all three verification
  reads, successful begin clearing, and probe-only retry preservation.
- `strict-readback-test-agent`: confirmed CONFIG, `LO_THRESH`, and `HI_THRESH`
  strict mismatch tests assert `READBACK_MISMATCH`, observed `Status::detail`,
  CONFIG OS-bit masking, and structural dirty diagnostics.
- `raw-register-contract-agent`: confirmed Option A raw-write behavior is
  recorded and tested. Added an in-test comment documenting that raw writes are
  diagnostic access that update hardware, leave typed cache unchanged, and mark
  cache/hardware state dirty.
- `compatibility-review-agent`: confirmed original Prompt 02 commit
  `7ce66d14cef39e5f2c6a7b497bcf02c65770c764` was bounded to tests, report, and
  `Status.h`; no production implementation was changed in that commit, current
  tests do not invalidate the test-first record, and no compatibility blocker
  remains.

Prompt 02 re-verification validation:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python -m platformio test -e native` | `108 test cases: 108 succeeded in 00:00:02.178`; environment `native`, status `PASSED` |

## Prompt 03 Implementation Summary

Implemented P0 status and dirty-diagnostic behavior:

- Failed `begin()` still leaves `isInitialized() == false` and clears cached
  config/runtime state, but no longer clears `hardwareConfigDirty()` or
  `hardwareConfigDirtyError()` after partial writes or strict read-back failure.
  Dirty state is cleared only by a later successful full apply.
- Strict read-back mismatches for `LO_THRESH`, `HI_THRESH`, and CONFIG now return
  `Err::READBACK_MISMATCH`. `Status::detail` carries the observed raw register
  value. CONFIG comparison still masks dynamic OS/status bits.
- Offline tracked I2C short-circuits now return `Err::OFFLINE` without touching
  the bus. `recover()` remains the explicit path allowed to access the bus while
  offline.
- `startConversion()` and `startConversion(mux)` return
  `Err::UNSUPPORTED_OPERATION` in continuous mode. A true already-active
  single-shot conversion still returns `Err::BUSY`.
- Successful raw writes to CONFIG, `LO_THRESH`, and `HI_THRESH` are treated as
  diagnostic writes: they return `Status::Ok()`, leave the typed cache unchanged,
  and mark `hardwareConfigDirty()` with `Err::HARDWARE_CONFIG_DIRTY` and the raw
  register pointer in `Status::detail`.
- Typed internal writers were routed around public raw-write dirty marking so
  `writeConfig()`, config setters, conversion start, shutdown, and
  `setThresholds()` keep their cache-commit semantics.
- Example status string helpers now print the new `Err` names instead of
  `UNKNOWN`.

Public API/status changes:

- Appended `Err::OFFLINE`, `Err::UNSUPPORTED_OPERATION`,
  `Err::READBACK_MISMATCH`, and `Err::HARDWARE_CONFIG_DIRTY`; existing enum
  values were not reordered.
- No new public accessors or `Status` fields were added. Failed-`begin()`
  partial-state diagnostics use existing `hardwareConfigDirty()`,
  `hardwareConfigDirtyError()`, and `SettingsSnapshot` fields.

Compatibility notes:

- Source and ABI shape are preserved: `Err` remains `uint8_t`, `Status` layout is
  unchanged, and enum additions are append-only.
- Callers that previously treated offline as `BUSY`, continuous-mode
  `startConversion()` as `BUSY`, or strict mismatch as `I2C_ERROR` now receive
  more precise status codes. This is an intentional P0 diagnostic hardening
  change.
- Raw public register writes are now explicitly diagnostic and set dirty state;
  typed helpers should be used for cache-synchronized writes.

Additional Prompt 03 regression tests:

- Strict low-threshold and high-threshold read-back mismatches now verify
  `READBACK_MISMATCH` and observed-register `detail`.
- Failed-begin dirty state now survives a later retry that fails during probe,
  proving dirty state is not cleared merely by starting a new `begin()`.

Prompt 03 crash-recovery verification on 2026-06-02:

- `core-status-agent`: confirmed appended status values remain order-compatible,
  example status string helpers cover the new names, offline tracked I2C
  short-circuits before bus access, `recover()` is the offline bus-access
  exception, continuous-mode `startConversion()` returns
  `UNSUPPORTED_OPERATION`, and `BUSY`/`IN_PROGRESS` remain distinct.
- `begin-dirty-agent`: confirmed failed `begin()` preserves dirty diagnostics
  through `hardwareConfigDirty()`, `hardwareConfigDirtyError()`, and
  `SettingsSnapshot` while leaving `_initialized == false`; dirty state clears
  through a later successful `begin()` full apply. It noted `recover()` remains
  unavailable after failed `begin()` because the driver is intentionally not
  initialized.
- `strict-readback-agent`: confirmed strict read-back mismatch handling for
  CONFIG, `LO_THRESH`, and `HI_THRESH`, observed raw register details, CONFIG
  OS-bit masking, and dirty diagnostic preservation. It noted begin transport
  failure coverage was less symmetric than recover coverage.
- `test-fault-agent`: confirmed current fault-injection coverage for Prompt 03
  contracts and a passing native suite; it noted direct BUSY coverage for a
  second pending single-shot start was indirect.
- `api-compatibility-agent`: confirmed no `Status` layout change, no new public
  dirty accessors or snapshot fields, no framework dependency in `include/` or
  `src/`, and Doxygen/README/example string-helper updates. It noted the core
  still intentionally has no public `Err`-to-string helper.
- `final-review-agent`: confirmed original Prompt 03 commit
  `effddc526ab51927488acb12ede1e6586cde00c0` exists on the remote branch and
  found no unresolved Prompt 03 P0 implementation issue.

Prompt 03 recovery test tightening:

- Expanded
  `test_begin_strict_readback_transport_failure_after_writes_preserves_dirty_diagnostic`
  so failed `begin()` preserves original transport status/detail and dirty
  diagnostics when strict read-back fails on any of the three verification reads:
  `LO_THRESH`, `HI_THRESH`, or CONFIG.
- Added
  `test_start_conversion_while_single_shot_pending_returns_busy_without_bus_access`
  to directly pin that an already-active single-shot conversion returns
  `Err::BUSY` without another bus write, preserving the
  `UNSUPPORTED_OPERATION`/`BUSY`/`IN_PROGRESS` distinction.

Remaining gaps after Prompt 03:

- Hardware validation evidence is still required before field-grade or
  production-readiness claims.
- P1 API/documentation work remained for bool-only readiness, `tick()` status
  visibility, `nowMs` scope, blocking latency, and broader guard/test coverage.

## Prompt 04 Raw Register Contract

Selected contract: public raw register writes are diagnostic-only. Successful
`writeRegister16()` / `writeRegister()` calls to CONFIG, `LO_THRESH`, or
`HI_THRESH` update hardware, leave typed cache unchanged, and mark
`hardwareConfigDirty()` with `Err::HARDWARE_CONFIG_DIRTY`. The dirty diagnostic
status stores the raw register pointer in `detail`; the raw write itself still
returns `Status::Ok()`. Invalid registers and read-only conversion register
writes are rejected before I2C. If a raw write reaches the transport and returns
an error, the dirty diagnostic preserves that transport status because hardware
may still have accepted the write. Offline short-circuits do not touch the bus
or mark dirty.

Implementation status:

- Core implementation already followed the selected contract from Prompt 03.
  Prompt 04 tightened dirty clearing so a dirty full apply performs read-back
  verification even when `strictInitVerify` is false.
- `recover()` clears raw-write dirty state only through a successful full cached
  config reapply plus read-back verification. A failed recover preserves dirty
  visibility; if a later partial apply reaches hardware and fails, the dirty
  diagnostic records that newer transport failure.
- Failed raw writes that reached the transport now mark dirty with the original
  transport status. Offline and validation errors still return before I2C and do
  not mark dirty.
- `getSettings()` exposes `hardwareConfigDirty` and `hardwareConfigDirtyError`
  so cached settings snapshots do not imply confirmed hardware synchronization
  after a raw diagnostic write.

Prompt 04 tests added:

| Test | Expected behavior |
| --- | --- |
| `test_write_register_alias_marks_hardware_config_dirty_without_cache_commit` | Compatibility alias inherits raw diagnostic dirty behavior and leaves threshold cache unchanged. |
| `test_failed_raw_write_marks_hardware_config_dirty_with_transport_error` | Failed raw transport write returns the original error and marks dirty because hardware may have accepted the write. |
| `test_raw_write_offline_returns_offline_without_dirty_or_bus_access` | Offline raw write short-circuits without bus access or dirty state changes. |
| `test_recover_success_clears_raw_register_dirty_after_full_resync` | Successful `recover()` reapplies cached settings and clears raw-write dirty state. |
| `test_recover_raw_dirty_requires_verified_readback_before_clear` | Dirty recovery must pass read-back verification before clearing dirty state, even when strict init verification is disabled. |
| `test_failed_recover_probe_preserves_raw_register_dirty_reason` | Probe/read failure during `recover()` does not clear or replace the original raw-write dirty reason. |
| `test_failed_recover_partial_apply_keeps_raw_register_dirty_visible` | Partial apply failure during `recover()` keeps dirty visibility and records the new transport failure. |

Documentation updates:

- Doxygen for raw writes documents invalid-register rejection, alias behavior,
  and dirty diagnostic detail.
- README raw diagnostics now documents the register pointer in dirty diagnostic
  detail.
- CLI help for `wreg` now says diagnostic writes mark cache dirty.

Prompt 04 re-verification on 2026-06-02 at current tip:

- `raw-register-contract-agent`: confirmed Option A is explicit in
  `writeRegister16()` and inherited by `writeRegister()`: public raw writes to
  writable registers mark `HARDWARE_CONFIG_DIRTY` with register-pointer dirty
  detail, while invalid and conversion-register writes are rejected before I2C.
- `cache-consistency-agent`: confirmed `getSettings()` exposes dirty state,
  typed setters do not accidentally use public raw dirty marking, dirty state
  forces read-back during full apply, and failed recovery preserves dirty
  visibility.
- `tests-agent`: confirmed native tests cover raw CONFIG/`LO_THRESH`/`HI_THRESH`
  dirty behavior, alias behavior, invalid/read-only rejection, failed raw write,
  offline short-circuit, successful recover clearing, read-back-required
  clearing, and failed recover preservation.
- `docs-agent`: confirmed README, Doxygen, CLI help/runtime warning, and CLI
  guard coverage. It identified and this entry corrects the report wording that
  could imply returned `Status::detail` carries the raw register pointer.
- `final-review-agent`: found no blocking Prompt 04 P0 issue and confirmed the
  original Prompt 04 commit `8056c962a010b4db83551d2c4805514899886ee5` is on
  the branch with bounded scope.

Remaining gaps after Prompt 04:

- No unresolved Prompt 04 P0 issue is known.
- Dated hardware/HIL evidence is still required before field-grade validation
  claims.

## Prompt 05 API Timing Contract

API additions and compatibility decisions:

- Kept existing `bool conversionReady()` as a source-compatible lossy
  convenience helper. It still returns `false` for both "not ready" and
  readiness-path failures.
- Added `Status conversionReady(bool& ready)` as an explicit-status alias for
  `readConversionReady(bool&)`.
- Kept existing `void tick(uint32_t nowMs)` as a compatibility wrapper.
- Added `Status service(uint32_t nowMs)` for the same bounded service step with
  immediate error visibility.
- Added `SettingsSnapshot::timebaseAvailable`; it is `true` when `Config::nowMs`
  is configured. When false, health timestamps that read as `0` are unavailable,
  not real measured times.

Timing and no-clock contract:

- `Config::nowMs` remains optional for `begin()`. Blocking reads require it and
  return `Err::INVALID_CONFIG` before starting a conversion when it is missing.
- Without `Config::nowMs`, direct timing-based readiness does not advance by
  elapsed time. Applications can drive pending conversions with
  `tick(nowMs)`/`service(nowMs)` from an external scheduler timebase. The
  ALERT/RDY GPIO readiness path remains supported in no-clock mode once that
  external service timebase has anchored and advanced the pending conversion.
- `tick()`/`service()` may perform one tracked CONFIG read when a single-shot
  conversion is pending and its data-rate interval has elapsed. `tick()` ignores
  the immediate `Status`; failures remain visible through health state,
  counters, `lastError()`, and `lastErrorMs()` when a timebase is available.
- `readBlocking()` in continuous mode returns the current/latest conversion
  register value immediately after the `nowMs` precondition check; it does not
  wait for a fresh continuous sample.
- Single-shot `readBlocking()` polls OS readiness at most once per observed
  millisecond tick. If the injected clock stalls, it returns `Err::TIMEOUT`
  after a finite same-tick guard instead of spinning forever. `cooperativeYield`
  remains optional and is a no-op unless supplied.

Prompt 05 tests added:

| Test | Expected behavior |
| --- | --- |
| `test_conversion_ready_status_alias_preserves_transport_error` | New readiness alias preserves exact transport status while the bool helper remains lossy. |
| `test_service_returns_ready_poll_failure_and_updates_health` | `service(nowMs)` returns immediate poll failure and updates health diagnostics. |
| `test_tick_discards_status_but_updates_health_on_ready_poll_failure` | `tick(nowMs)` keeps source-compatible void behavior while tracked I2C failure updates health. |
| `test_no_clock_direct_readiness_waits_for_service_timebase` | No-clock direct readiness stays conservative until service anchors and advances external time. |
| `test_no_clock_alert_ready_pin_uses_external_service_timebase` | No-clock ALERT/RDY readiness stays conservative until the external service timebase passes the conversion interval, then reads GPIO without CONFIG polling. |
| `test_no_clock_health_timestamps_are_marked_unavailable` | Snapshot exposes missing timebase so timestamp `0` is not presented as a real time. |
| `test_tick_marks_continuous_ready_without_config_poll_after_interval` | Continuous-mode service readiness advances without CONFIG polling. |
| `test_read_blocking_continuous_returns_latest_immediately_without_fresh_wait` | Continuous-mode `readBlocking()` reads the latest conversion register immediately without starting or waiting for a fresh conversion interval. |
| `test_single_shot_elapsed_os_busy_remains_not_ready` | Elapsed single-shot readiness still respects OS busy read-back. |
| `test_read_blocking_success_uses_cooperative_yield_cadence` | Blocking read succeeds with cooperative-yield-driven time advance. |
| `test_read_blocking_tolerates_repeated_same_tick_before_clock_advances` | Repeated same millisecond observations before clock advance do not false-timeout. |
| `test_read_blocking_polls_ready_state_once_per_observed_tick` | Single-shot blocking readiness polls CONFIG at most once per observed tick. |
| `test_read_blocking_times_out_with_advancing_clock_while_os_busy` | Advancing-clock OS-busy path times out and clears stale conversion state. |
| `test_read_blocking_propagates_ready_poll_transport_error` | Blocking readiness poll transport errors propagate exactly. |

Compatibility notes:

- No status enum values or `Status` fields were added or reordered.
- Adding non-virtual member functions does not change driver instance layout.
- Adding the overloaded `conversionReady(bool&)` can make uncast member-function
  pointer expressions for `conversionReady` ambiguous; normal calls to
  `conversionReady()` remain source-compatible.
- Adding `SettingsSnapshot::timebaseAvailable` changes that public snapshot
  struct layout and positional aggregate initialization shape. Normal
  `getSettings()` field access remains source-compatible, but ABI-sensitive
  consumers or aggregate initializers should be recompiled/updated.

## Validation Log

Prompt 01 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h` |
| `python -m platformio test -e native` | `57 test cases: 57 succeeded in 00:00:02.147`; environment `native`, status `PASSED` |

Prompt 01 crash-recovery verification on 2026-06-02:

Recovery startup commands:

| Command | Result |
| --- | --- |
| `git status --short` | clean worktree; empty output |
| `git branch --show-current` | `hardening/ads1115-industry-standard-p0` |
| `git log --oneline -5` | `7d6032e docs: finalize ADS1115 industry-standard hardening report`; `c4558b2 docs: add ADS1115 integration and HIL validation plan`; `73f87fe test: expand ADS1115 edge coverage and core guards`; `3f4583b docs: clarify ADS1115 readiness and service timing contracts`; `8056c96 fix: mark ADS1115 raw register writes as cache-dirty` |

- `repo-state-agent`: confirmed a clean worktree, current branch
  `hardening/ads1115-industry-standard-p0`, remote tracking branch in sync, and
  Prompt 01 commit `38e55ea` present with only `AGENTS.md` and this report
  changed.
- `audit-gap-agent`: confirmed required `AGENTS.md` rules, report structure,
  Prompt 01 results row, and recorded Prompt 01 validation evidence; noted that
  later prompts have since replaced the original final-report placeholder.
- `datasheet-contract-agent`: confirmed the no-chip-ID, plausibility-only
  read-back, dirty/partial hardware-state, raw diagnostic write, append-only
  status-code, and transport-error visibility rules are captured.
- `implementation-planner-agent`: confirmed branch, starting commit, source
  audit report path, chunk plan, rules, prompt results table, and final report
  section are present, with Prompt 01 still recorded as docs/rules only.
- `final-review-agent`: confirmed branch baseline, Prompt 01 commit scope,
  required `AGENTS.md` rules, report initialization, validation evidence, and
  remote sync; noted the only repo-auditability gaps were the lost original
  subagent and startup transcripts, now recorded in this recovery note.

Prompt 01 recovery validation on `hardening/ads1115-industry-standard-p0` at
pre-recovery tip `7d6032e`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; metadata aligned at `1.0.0` |
| `python -m platformio test -e native` | `106 test cases: 106 succeeded in 00:00:02.722`; environment `native`, status `PASSED` |

Prompt 02 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python -m platformio test -e native` | `ERRORED`; `67 test cases: 13 failed, 53 succeeded in 00:00:02.286` |

Historical Prompt 02 expected native failures retained as test-first evidence;
these are not current failures:

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

Prompt 02 recovery validation on `hardening/ads1115-industry-standard-p0` after
contract test tightening:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python -m platformio test -e native` | `107 test cases: 107 succeeded in 00:00:02.465`; environment `native`, status `PASSED` |

Prompt 03 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h` |
| `python -m platformio test -e native` | `69 test cases: 69 succeeded in 00:00:01.990`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:24.048` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:23.496` |

Prompt 03 recovery validation on `hardening/ads1115-industry-standard-p0` after
strict begin read-back transport and single-shot `BUSY` test tightening:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; metadata aligned at `1.0.0` |
| `python -m platformio test -e native` | `108 test cases: 108 succeeded in 00:00:01.618`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:13.434` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:14.958` |

Prompt 04 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python -m platformio test -e native` | `76 test cases: 76 succeeded in 00:00:01.855`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:16.632` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:15.575` |

Note: an earlier non-verbose `esp32s2dev` build attempt in this prompt stopped
at `firmware.elf Error 1` without linker diagnostics. A verbose rerun succeeded
(`00:00:14.299`), followed by the exact required non-verbose command succeeding
as recorded above.

Prompt 04 re-verification validation on `hardening/ads1115-industry-standard-p0`
after later Prompt 02/03 recovery commits:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python -m platformio test -e native` | `108 test cases: 108 succeeded in 00:00:12.629`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:45.723` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:15.207` |

Prompt 05 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h` |
| `python -m platformio test -e native` | `88 test cases: 88 succeeded in 00:00:02.831`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:23.154` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:25.189` |

Note: an earlier `esp32s2dev` run during Prompt 05 stopped while compiling
framework Arduino `esp32-hal-tinyusb.c.o` with `Error 1` and no compiler
diagnostic in the captured output. Re-running the exact required command
succeeded as recorded above.

Prompt 05 recovery re-verification on `hardening/ads1115-industry-standard-p0`
after crash recovery and gap-fill tests:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `110 test cases: 110 succeeded in 00:00:01.685`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:13.599` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:13.253` |

Prompt 06 implementation on `hardening/ads1115-industry-standard-p0`:

- Native Unity tests expanded from 88 to 106 registered cases.
- Added focused native coverage for invalid I2C address/config enum/ALERT-RDY
  config rejection, tracked I2C status taxonomy, strict recover read-back
  mismatch and transport-failure branches, signed threshold reconstruction,
  all-gain LSB/scaling boundaries, config/comparator rollback variants, and
  dirty-diagnostic preservation.
- Expanded `tools/check_core_timing_guard.py` to scan stripped core code in
  `include/` and `src/` for timing calls, framework includes/symbols,
  ESP-IDF/FreeRTOS/logging leakage, and heap/STL allocation patterns.
- Expanded `scripts/generate_version.py check` to verify version metadata across
  `library.json`, `idf_component.yml`, `Doxyfile`, and generated `Version.h`.
- Documentation now avoids production-ready overclaims, records hardware
  validation as pending evidence, clarifies ADDR strap mapping and SDA strap
  timing, documents pull-up sizing caveats, corrects differential MUX wording,
  and keeps the ALERT/RDY approximately 8 us continuous-mode pulse caveat.

Prompt 06 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `106 test cases: 106 succeeded in 00:00:02.500`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:17.975` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:15.300` |
| `python -m platformio pkg pack` | Exit code 0; wrote `C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\ADS1115-1.0.0.tar.gz`; artifact removed after validation |

Prompt 06 recovery gap-fill on `hardening/ads1115-industry-standard-p0` after
crash recovery:

- Native Unity coverage increased from 110 to 112 registered cases in this
  recovery pass. The original Prompt 06 expansion remains 88 to 106 cases.
- Expanded `begin()` invalid-enum coverage to include `ComparatorMode`,
  `ComparatorPolarity`, and `ComparatorLatch` validation without bus access.
- Added direct `disableComparator()` rollback coverage on CONFIG write failure.
- Added representative dirty-preservation coverage for failed config-only and
  comparator setters while a prior raw CONFIG dirty diagnostic is already
  visible.
- Expanded `tools/check_core_timing_guard.py` ESP-IDF include rejection to cover
  additional IDF-only core headers: `sdkconfig.h`, `soc/`, `hal/`, `rom/`,
  `lwip/`, and `nvs_flash.h`.
- Docs/release wording was re-checked: README/CHANGELOG avoid production-ready
  overclaims, validation evidence remains pending where hardware logs are
  missing, ADDR strap/pull-up/differential MUX/ALERT-RDY/PGA caveats remain
  present, and current version metadata remains intentionally unbumped at
  `1.0.0` on this hardening branch.

Prompt 06 recovery validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `112 test cases: 112 succeeded in 00:00:02.412`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:15.527` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:13.813` |
| `python -m platformio pkg pack` | Exit code 0; wrote `C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\ADS1115-1.0.0.tar.gz`; artifact removed after validation |

Prompt 07 implementation on `hardening/ads1115-industry-standard-p0`:

- Arduino diagnostic CLI runtime output now labels itself as diagnostic, shows
  the global Wire timeout policy, uses `service()` from the main loop, reports
  dirty/cache diagnostics in `cfg/settings`, and warns on `wreg` raw writes.
- ESP-IDF example comments and `docs/IDF_PORT.md` now document coarse
  `esp_err_t` mapping limitations and explicitly avoid claiming precise
  address-NACK/data-NACK taxonomy without bus evidence.
- CI workflow now runs `python scripts/generate_version.py check` in addition to
  existing native tests, core guard, CLI guard, IDF example guard, Arduino
  ESP32-S2/S3 builds, ESP-IDF S2/S3 container builds, and package validation.
- Added `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md` and
  `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md` for HIL execution.
- Added `tools/hil_ads1115_capture.py`, an optional serial transcript helper
  with dry-run command listing and timestamped log capture; it does not declare
  pass/fail.

Prompt 07 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `106 test cases: 106 succeeded in 00:00:02.163`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:14.932` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:15.630` |
| `idf.py` availability check | `idf.py unavailable`; local pure ESP-IDF builds were not run |
| `python tools/hil_ads1115_capture.py --dry-run --suite identity` | Exit code 0; printed local branch, local commit, and command list `version`, `addr`, `state`, `cfg`, `drv` |

CI/build evidence after Prompt 07:

- `.github/workflows/ci.yml` configures Arduino PlatformIO builds for
  `esp32s3dev` and `esp32s2dev`.
- `.github/workflows/ci.yml` configures native tests, core guard, CLI guard, IDF
  example guard, version metadata check, package validation, and pure ESP-IDF
  example container builds for `esp32s3` and `esp32s2`.
- The workflow now runs on `main` pushes, `hardening/**` branch pushes, pull
  requests targeting `main`, and manual `workflow_dispatch` runs. This branch
  has local command evidence until a CI run URL is recorded.

Remaining gaps after Prompt 07:

- Dated hardware/HIL logs or captures are still required before field-grade
  validation claims.
- Package archive contents are validated by `pio pkg pack` success, but there is
  not yet a separate package-contents allow/deny guard.

Prompt 07 recovery re-verification on `hardening/ads1115-industry-standard-p0`
after crash recovery and later Prompt 06 gap-fill:

- CI workflow trigger coverage was expanded from `main`-only push/PR triggers
  to include `hardening/**` branch pushes and manual `workflow_dispatch` runs.
- README CI evidence text now states the same trigger scope.
- Arduino and ESP-IDF example audits found no blocking gaps. The remaining
  caveats are honest: Arduino glue uses global `Wire.setTimeOut()`, ESP-IDF
  error mapping remains coarse unless instrumented, and current CI evidence is
  local/configuration evidence until a run URL is recorded.
- HIL plan/template/helper were re-verified. Results remain pending; no
  hardware pass/fail result was fabricated.

Prompt 07 recovery validation:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `112 test cases: 112 succeeded in 00:00:01.668`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:15.016` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:14.001` |
| `idf.py --version` | `idf.py : The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.` Local pure ESP-IDF builds were not run. |
| `python tools/hil_ads1115_capture.py --dry-run --suite identity` | Exit code 0; printed branch `hardening/ads1115-industry-standard-p0`, commit `10016a276f42c5d6c8fb168dcce4fa68e67150f9`, and commands `version`, `addr`, `state`, `cfg`, `drv` |

Prompt 07 repeated verification on `hardening/ads1115-industry-standard-p0`
after final-report freshness patch:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing/framework guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h`; `Version metadata aligned: library.json=1.0.0, idf_component.yml=1.0.0, Doxyfile PROJECT_NUMBER=1.0.0, Version.h=1.0.0` |
| `python -m platformio test -e native` | `112 test cases: 112 succeeded in 00:00:02.422`; environment `native`, status `PASSED` |
| `python -m platformio run -e esp32s3dev` | `SUCCESS`; environment `esp32s3dev`, duration `00:00:22.598` |
| `python -m platformio run -e esp32s2dev` | `SUCCESS`; environment `esp32s2dev`, duration `00:00:20.215` |
| `idf.py --version` | `idf.py : The term 'idf.py' is not recognized as the name of a cmdlet, function, script file, or operable program.` Local pure ESP-IDF builds were not run. |
| `python tools/hil_ads1115_capture.py --dry-run --suite identity` | Exit code 0; printed branch `hardening/ads1115-industry-standard-p0`, commit `80095e688e9432bd248e58c07b1511eb229a524e`, and commands `version`, `addr`, `state`, `cfg`, `drv` |

## Final Report

Final release-readiness conclusions are recorded in
`docs/ADS1115_INDUSTRY_STANDARD_FINAL_REPORT.md`.
