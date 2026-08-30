# Code Audit Resolution Report

Date: 2026-08-30

Branch reviewed: `main`
Starting commit: `39dccd5db8d6838d193c7bd17a4e13bbcaabe17e`
Fresh-audit baseline: `ab9f6a4a97ae06a3ed603095057e87c9d5209e88`

## Scope and method

The branch was fetched, pruned, and fast-forward checked against `origin/main`
before each review; it was current and clean at both baselines. Every finding in
the former `docs/CODE_AUDIT.md` was read again in full and checked against the
implementation, public contracts, tests, examples, packaging, and HIL tooling.

After the first resolution commit, three independent parallel reviews examined:

- the original requirements and finding-by-finding coverage;
- core implementation correctness, timing, cancellation, and edge cases;
- tests, examples, HIL coverage, scope, and code simplicity.

Their observations were then reproduced against the actual source and the
`39dccd5..ab9f6a4` diff. No finding was accepted from a prior summary alone. The
follow-up found several real gaps in the first pass, listed below, and the final
suite revalidated both the original fixes and the follow-up corrections.

The review favored the smallest change that preserves the owner-polled design,
transport injection, bounded work, and existing public compatibility surface.
Proposals that would create duplicate APIs, change intentional semantics, or
claim hardware coverage that the repository cannot provide were rejected and
are called out below.

## Fresh-audit corrections

The independent follow-up confirmed and corrected these remaining gaps:

- Single-shot timing was anchored before a potentially blocking start callback.
  Direct starts now sample time after the callback; owner starts arm their full
  guard interval on the next `poll()` boundary. Continuous settling is likewise
  armed only after the final verification callback has returned. Wraparound and
  delayed-callback regressions cover both paths.
- Shutdown verified only writable CONFIG bits and could report success while OS
  still said conversion busy. Continuous shutdown now waits a complete
  worst-case interval from a post-write owner boundary, requires an OS-idle
  readback, and retries a persistent busy indication within the operation
  deadline. Zero-budget polls and verification failures preserve that guard,
  and an unknown or dirty pre-shutdown profile uses the conservative 8-SPS
  interval. The synchronous facade requires `Config::nowMs` only for continuous
  or untrusted state; its verified clean single-shot path remains clockless and
  returns boundedly with reconciliation active if the post-write read observes
  busy hardware or fails. Advancing-clock timeout and stopped-clock paths also
  retain bus-silent reconciliation.
- A failed direct readiness read or CONFIG mismatch left a compatibility
  conversion latched active, preventing recovery. Those terminal error paths now
  discard the abandoned sample and release the latch while preserving transport
  health and dirty-state diagnostics.
- The native fake still lacked OS/conversion lifecycle behavior and its reset
  helper left sticky fault controls behind. A small opt-in conversion model,
  post-callback time advancement, persistent-busy shutdown coverage, continuous
  deasserted-GPIO coverage, and complete fault-control reset were added without
  turning the fake into a speculative ADC simulator.
- Comparator mode, polarity, and latch enum mappings were missing from the
  compile-time register checks. They are now asserted alongside the existing
  queue, MUX, PGA, rate, mode, mask, and pointer mappings.
- The production owner example budget allowed no readiness retry. It now budgets
  four callbacks: start, initial OS check, one bounded OS retry, and conversion
  read.
- The diagnostic HIL path still wrote `0x8583`, which can request a conversion,
  and it did not exercise the owner API. The safe raw write is now `0x0583`, and
  bounded `own bind/init/read/poll/cancel/recover/shutdown/unbind` commands are
  present in the Arduino diagnostic CLI and targeted HIL plan. HIL validators
  require the exact pending/done, zero-callback, cancellation, and bound/unbound
  states instead of accepting a generic section header or success line. The
  plan explicitly restores and verifies the clocked compatibility binding
  afterward so its later full, exhaustive, and soak reads remain valid.

No public enum, status value, or API type was added, so these corrections remain
a patch-level bug-fix and verification update at version 2.0.1.

## Finding dispositions

| Finding | Verdict | Resolution |
| --- | --- | --- |
| 2.1 `getThresholds()` corrupts desired configuration | Valid | Made the method a pure diagnostic observer. It returns hardware values and invalidates `VERIFIED` trust on mismatch, but does not modify cached, desired, or applied thresholds. Recovery now reliably restores the committed profile. |
| 2.2 OS-busy readiness retry floods I2C | Valid | Retry cadence is now one eighth of the worst-case conversion interval, rounded up to milliseconds. At 8 SPS this is 18 ms rather than 1 ms. Wrap-safe, bus-silent gate, and successful retry behavior are covered. |
| 2.3 initialization transport is absent from health | Valid | Every tracked callback now updates counters, timestamps, and last error, including initialization. `DriverState` alone remains `UNINIT` until initialization succeeds. |
| 2.4 initialize/recover probe inconsistency | Valid | Both owner initialization and recovery use a tracked CONFIG read and map an address NACK to the operation result `DEVICE_NOT_FOUND`. Health retains the underlying `I2C_NACK_ADDR`, which is the useful transport diagnostic. Public `probe()` remains raw and untracked by contract. |
| 2.5 blocking timeout cleanup is hidden | Partly valid documentation gap | Kept the existing token/poll cleanup model; an overload would duplicate the already-exposed `activeOperationToken()`. Doxygen and README now state that both `TIMEOUT` and `CLOCK_STALLED` can leave `RECONCILING` work that the owner must poll and consume. |
| 2.6 dirty-error provenance can be overwritten | Partly valid; blanket preserve-first proposal rejected | Renamed the replacing helper to make its semantics explicit. The diagnostic records the latest hardware-affecting error in an operation, while a first ambiguous error is retained until better effect evidence exists. Definite address NACK on a raw write no longer dirties clean state. This is more accurate than preserving an unrelated older error forever. |
| 2.7 1024-iteration stopped-clock guard is too small | Valid | Raised the finite heuristic to 100,000 same-tick polls and documented its approximately 1 ms clock assumption. A regression advances only after more than 1,024 yields and completes without a false `CLOCK_STALLED`. |
| 2.8 conversion-ready recognition is too narrow | Valid | Validation and runtime recognition now implement the datasheet bit rule: low threshold MSB clear, high threshold MSB set, queue enabled. Mode, polarity, latch, queue depth, and remaining threshold bits are accepted. The convenience writer still emits the canonical profile. |
| 2.9 enable/disable/queue sequence becomes invalid | Valid consequence of 2.8; proposed threshold reset rejected | The broader datasheet-correct recognition fixes the sequence. `disableComparator()` continues to preserve thresholds, and `enableConversionReadyPin()` does not require a GPIO callback because chip output can be consumed externally. Sequence coverage includes ASSERT_2, window, and latching settings. |
| 2.10 shutdown leaves desired mode continuous | Valid | Accepted shutdown immediately enters `APPLYING`; verified success commits single-shot mode to desired, applied, and compatibility caches. A definite pre-write failure restores prior trust, while ambiguous failures remain unknown. Read and recovery after shutdown now retain single-shot mode. |
| 2.11 reads advance configuration generation | Intentional, not a defect | Each typed read writes and verifies its requested MUX/PGA. A generation shared by samples with different verified hardware profiles would be misleading. The existing sample sequence supplies sample identity, so no second speculative generation API was added. |
| 2.12 owner path lacks clock/GPIO/offline knobs | Mostly incorrect; documentation clarified | Owner health timestamps come only from `poll(nowMs)`, owner reads intentionally use CONFIG OS polling rather than GPIO, and the passive threshold remains fixed at five. `DriverConfig` now states all three contracts; no unused hooks were added. |
| 2.13 portability and packaging claims are too broad | Partly valid | Lowered the framework-neutral core, CMake propagation, native tests, and packed-core compile gate to C++11. Added ESP-IDF component exclusions for repository-only CI, docs, tests, tools, and scripts. Kept ESP32-S2/S3 and `espressif32` target scope because those are explicit project requirements. Used `files.exclude`; the [Component Manager manifest rules](https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html#files) define `files.include` as a re-inclusion filter, not an allow-list. |
| 2.14 `setThresholds()` cannot program ready mode | Intentional API separation | Ordinary comparator thresholds require `high > low`; the atomic `enableConversionReadyPin()`/profile path owns the special ready encoding. No duplicate or ambiguous setter mode was added. |
| 2.15 legacy staged start methods discard their local token | Compatibility design, with documentation gap | Poll results already expose the token and examples consume `result.token`. Adding overloads would expand a legacy surface without a new capability. Doxygen now states exact start preconditions and results. |
| 2.16 idle specialized pollers return success | Valid | `pollSingleShot()` and `pollApplyConfig()` now return `RESULT_NOT_AVAILABLE`, `done=true`, and perform zero callbacks when idle. Unbound and wrong-operation behavior remains distinct. |
| 2.17 owner start preconditions are inconsistent | Mostly intentional; one real observer bug | Initialization/recovery intentionally require a binding; apply/read/shutdown require successful initialization. Those contracts are now explicit. `getAppliedProfile()` now leaves its output untouched when unbound. |
| 2.18 compatibility fields and aliases appear unused internally | Not a current defect | `strictInitVerify`, register aliases, and snapshot operation tokens are retained compatibility/diagnostic surfaces. Deprecating or removing them would create warnings or an unnecessary breaking change. Any removal belongs in an explicit major-version migration. |
| 2.19 `UINT32_MAX` is both timestamp and sentinel | Valid | Added an explicit private timestamp-valid flag and removed the sentinel encoding. A real start at `UINT32_MAX`, wraparound readiness, no-clock service arming, rollback, and unbind behavior are covered. |
| 2.20 generated version metadata is not reproducible | Valid | Generated-header fallbacks are deterministic `"unknown"` values instead of `__DATE__`/`__TIME__`. PlatformIO metadata defaults to the same deterministic values and honors `SOURCE_DATE_EPOCH` in UTC when supplied. Public macro and constant names remain compatible. |
| 2.21 register constants can drift from enums | Valid | Added compile-time assertions for register pointers, masks/shifts, MUX, PGA including aliases, data rate, mode, comparator mode/polarity/latch/queue, OS, and whole-register masks. No runtime or API cost was introduced. |
| 2.22 native fake is too permissive | Valid in the reported areas; full ADC simulator rejected | The fake enforces the configured address, rejects conversion-register writes, applies explicitly ambiguous failed writes, models callback time, and has a small opt-in CONFIG/OS conversion lifecycle. Its reset helper now clears sticky statuses and all read masks. This is sufficient to cover the reproduced timing and shutdown defects without creating an analog simulator. |
| 2.23 HIL runner and CLI checker contain dead/unsafe paths | Valid except formatter subproposal | Removed the unused `failure_tokens` field and unreachable `UNKNOWN` result class, retaining `--fail-on-unknown` as a compatibility alias for evidence-required failure. The raw CONFIG exercise now writes `0x0583`, which does not request a single-shot conversion, and complete nominal restoration remains after optional benchmarks. The CLI checker validates real dispatch predicates and the owner command surface. A new clang-format gate was not added because no formatter version or tracked-file baseline is pinned. |
| 2.24 owner-safe lifecycle lacks automated hardware commands | Valid | The production owner example retains its mutex, scheduler, bounded recovery, and token handling. Separately, the diagnostic Arduino CLI now exposes bounded `own` lifecycle commands, and the targeted HIL plan exercises normal reads, post-write cancellation/reconciliation, recovery, shutdown, unbind/rebind, and final initialization. This creates a hardware exercise path without pretending the diagnostic CLI is a production bus manager. Physical transport-fault and ALERT/RDY validation remain dated external evidence items. |

## Resulting changes

- Corrected configuration trust, health accounting, readiness scheduling,
  shutdown state, timestamp representation, and compatibility poll semantics.
- Added complete compile-time register-contract checks and 25 focused native
  regressions across both passes; the suite increased from 178 to 203 tests.
- Made the core C++11-consumable without lowering the C++17 example builds.
- Made generated build metadata reproducible and reduced ESP-IDF package scope.
- Hardened fake transport, CLI dispatch checking, HIL classification, raw-write
  safety, final profile restoration, owner-safe hardware exercise, and the
  production owner example's recovery and callback budgeting.
- Updated Doxygen, README, IDF guidance, changelog, and repository engineering
  rules to match the verified behavior.

## Verification

The final change was checked with:

- native PlatformIO tests: 203/203 passed under C++11;
- host `g++ -std=c++11 -Wall -Wextra -Wpedantic -Werror` core compile, with
  the same pedantic gate added to packaged-core CI;
- Arduino builds for diagnostic and owner-safe examples on ESP32-S2 and ESP32-S3;
- core timing/framework, CLI contract, ESP-IDF example, and version checks;
- HIL parser self-test, a 238-step two-address targeted dry run, and a 544-step
  two-address exhaustive-plus-benchmark dry run;
- PlatformIO package creation, unpack, generated-header presence, and packed-core
  C++11 pedantic compile;
- ESP-IDF manifest YAML parse and Doxygen warning-enforced generation.

No new physical hardware validation was performed, so this report makes no new
electrical, timing, analog-accuracy, or fault-injection claims.
