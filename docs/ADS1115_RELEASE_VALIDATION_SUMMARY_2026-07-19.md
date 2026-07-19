# ADS1115 2.0 TunnelMonitor Suitability Hardening Report

## Outcome

The focused library hardening is complete on
`hardening/tunnelmonitor-suitability-reaudit`. The immutable implementation
commit is `5c1ee185158ef8593cee3d1c54bea4e90880205b` and is synchronized to
`origin/hardening/tunnelmonitor-suitability-reaudit`.

The library-side findings ADS-TM-02 through ADS-TM-11 and ADS-TM-13 are
resolved by the 2.0 contracts. ADS-TM-01, ADS-TM-12, and ADS-TM-14 remain
TunnelMonitor product/electrical decisions. ADS-TM-15 remains partially open
for current physical HIL and final-board acceptance. No TunnelMonitor firmware
was edited.

This is software/build evidence, not a physical-hardware validation claim.

## Scope And Preserved State

- ADS1115 baseline: `v1.2.0`,
  `9c9ade807f18eabd534423c64b2dfe02efe43de4`
- TunnelMonitor-node inspected read-only at
  `602114ea6c723e31c41f0eb7cd8ac2b56a46d40e`
- TunnelMonitor user changes preserved: modified `.vscode/extensions.json` and
  untracked
  `docs/reports/i2c_library_latest_branch_audit_revalidation_20260718.md`
- Core remains framework-neutral, transport-injected, and non-owning
- No bus, pin, clock, lock, task, retry, recovery, or logging ownership was
  added to the library

Hardening review was split across core contracts, fault injection,
documentation/examples/CI, and an independent final integration review. The
final integration review returned GO with no remaining high- or
medium-severity software blocker and made no repository edits.

## Commit Record

| Commit | Purpose |
|---|---|
| `f48af452ce9190e9d7b728cd449ccf8db70444d5` | Preserve the imported suitability audit baseline |
| `9e2e0e5f7544c22a5d4758ca557e467e0de1b6a5` | Add the fixed-memory owner-safe operation engine |
| `e9f01d6afa38edce19277719c38685a89eabff9b` | Harden cancellation stages and Arduino compatibility |
| `cc88a8f6cd43fcd60d44cc499f0df6ee997ec251` | Publish the 2.0 integration contract and release metadata |
| `5c1ee185158ef8593cee3d1c54bea4e90880205b` | Close final lifecycle, deadline, CLI, HIL, and trust gaps |

Every implementation chunk was committed and pushed before the next evidence
step.

## Implemented Production Contract

The new fixed-memory owner path is
`bind()` / `start*()` / `poll()` / `takeResult()` / `unbind()`:

- Start, cancel, result consumption, and unbind are bus-silent.
- `poll()` is the only owner-safe transport entry point and accepts a callback
  budget clamped to three; a zero budget is bus-silent.
- Remaining deadline time is conservatively partitioned across a multi-callback
  poll, and every callback is also capped by `transferTimeoutMs`.
- Initialization and recovery perform a CONFIG reachability probe, three
  writes, and three readbacks. ADS1115 has no identity register, so this is
  plausibility/readback verification rather than identity proof.
- One active operation and one pending terminal result are stored without heap
  allocation. Nonzero tokens enforce exactly-once result consumption.
- A successful sample atomically carries raw code, checked rounded
  microvolts, channel ID, MUX, PGA, rate, flags, configuration generation, and
  sequence.
- Confirmed or ambiguous abandoned conversions enter bus-silent reconciliation.
  The quiet interval is armed at the first trustworthy owner poll after the
  blocking start callback returns, preventing delayed transport completion from
  shortening the guard.
- Configuration trust is explicit. Partial, raw, cancelled, timed-out, or
  mismatched effects remain `UNKNOWN`/dirty until a full verified replay.
- Health counters and `OFFLINE` are passive diagnostics; they do not deny an
  owner-authorized transport operation.
- Production owner reads are single-shot OS polling. Continuous latest-register
  reads and ALERT/RDY GPIO behavior remain documented diagnostics.
- `startShutdown()`/`shutdown()` provide fallible verified hardware idle;
  `unbind()`/`end()` are bus-silent.

## Compatibility Impact

Version 2.0.0 is a deliberate major release.

- Prior `Err` numeric values are unchanged. New codes were appended:
  `CANCELLED`, `CONFIG_UNKNOWN`, `RESULT_NOT_AVAILABLE`, `TOKEN_MISMATCH`, and
  `INDETERMINATE`.
- `Config`, blocking calls, direct setters/register access, and staged jobs are
  retained for source migration and diagnostics.
- `begin()` and recovery now always verify all writable profile registers.
- Direct typed mutations move full-profile trust to `UNKNOWN` until a complete
  apply/recover succeeds.
- Scaled/blocking reads require initialized, verified, clean, single-shot
  configuration. Continuous scaled reads return `UNSUPPORTED_OPERATION`;
  `readLatestRaw()`/continuous raw diagnostics remain available.
- Compatibility staged jobs leave a tokened terminal result pending. Callers
  must use `takeResult(pollResult.token, ...)` before another operation. Both
  diagnostic CLIs now do this automatically.
- `end()` no longer performs an implicit hardware write.
- Driver instances remain externally serialized, non-thread-safe, and
  non-ISR-safe; copy and move are deleted.

## Verification Evidence

Validation date: 2026-07-19. PlatformIO Core was `6.1.19`; Doxygen was
`1.15.0`. PlatformIO Native is pinned to `1.2.1`, pioarduino espressif32 to the
exact `54.03.20` archive, and the ESP-IDF CI image to the documented immutable
v5.3.5 digest. Host runner/compiler variability remains explicit.

| Check | Result |
|---|---|
| `python -m platformio test -e native` | PASS, 174/174 |
| `python tools/check_core_timing_guard.py` | PASS |
| `python tools/check_cli_contract.py` | PASS |
| `python tools/check_idf_example_contract.py` | PASS |
| `python scripts/generate_version.py check` | PASS; all metadata at 2.0.0 |
| `python tools/run_i2c_hil.py --parser-test` | PASS |
| Targeted 0x48/0x49 HIL dry-run | PASS as dry-run; 180 unique planned steps |
| Doxygen generation | PASS with no warnings |
| PlatformIO package | PASS; 120,428-byte archive inspected, then removed |
| Independent final integration review | GO; no high/medium software blocker |

Final Arduino build measurements:

| Environment | Flash | RAM | Result |
|---|---:|---:|---|
| Diagnostic ESP32-S3 (`esp32s3dev`) | 412,574 B | 22,560 B | PASS |
| Diagnostic ESP32-S2 (`esp32s2dev`) | 405,209 B | 37,008 B | PASS |
| Owner-safe ESP32-S3 (`owner_safe_s3`) | 374,538 B | 22,616 B | PASS |
| Owner-safe ESP32-S2 (`owner_safe_s2`) | 346,309 B | 36,496 B | PASS |

The diagnostic CLIs now finish successful configuration commands with a
bounded full apply/readback, acknowledge staged terminal results, use latest
raw reads as the positive continuous-mode diagnostic, and explicitly expect
scaled continuous reads to be rejected. HIL plan tests enforce unique IDs and
the full six-transfer staged-apply sequence.

## Tests Added Or Strengthened

The 174-test native suite includes bounded/fault-injected coverage for:

- missing callbacks/clocks, stalled clocks, deadline wraparound, and zero-I2C
  validation failures;
- partial writes, applied-then-error writes, dirty-state preservation, and
  verified dirty-state clearing;
- mandatory masked readback and threshold/profile trust mismatch;
- one-operation/one-result admission, token mismatch, exactly-once consumption,
  and repeated compatibility staged jobs after acknowledgement;
- cancel before effects, cancel after each write stage, ambiguous conversion
  start, delayed blocking start callbacks, timeout reconciliation, and
  abandoned-sample suppression;
- per-poll transaction budgets, conservative multi-callback timeout
  partitioning, and bus-silent wait gates;
- typed sample provenance, fixed-size/trivially-copyable result contracts,
  checked integer scaling, and all PGA/data-rate/MUX boundaries;
- passive health transitions, owner `poll(nowMs)` timestamps, recovery while
  diagnostically offline, shutdown semantics, and copy/move prevention;
- continuous diagnostic timing tolerance/settling and explicit scaled-read
  status precedence.

## Remaining External Gates

The following are deliberately not claimed complete:

1. Local native ESP-IDF build: `idf.py` was unavailable on this host. CI is
   configured for ESP32-S2 and ESP32-S3 using the pinned image.
2. Current 2.0 physical HIL: existing COM19/COM8 captures predate 2.0. Parser
   tests and dry-runs are not hardware evidence.
3. Final-board electrical and analog validation: address strap, supply,
   pull-ups, source impedance, protection, absolute input limits, accuracy,
   saturation, disconnect behavior, and calibration.
4. Fault/shared-bus/workload validation: contention, unplug/replug, stuck bus,
   brownout, delayed/ambiguous transfers, cancellation, and production task
   cadence on the target board.
5. TunnelMonitor product decisions: replacement versus distinct source,
   required/optional role, board/profile, channel meanings, units, calibration,
   capacity, and product acceptance criteria.

Use `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md` and the results template for the
dated physical evidence required to close these gates.
