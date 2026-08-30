# Code Audit Resolution Report

Date: 2026-08-30

Branch reviewed: `main`
Starting commit: `39dccd5db8d6838d193c7bd17a4e13bbcaabe17e`

## Scope and method

The branch was fetched, pruned, and fast-forward checked against `origin/main`
before review; it was already current and clean. Every open finding in the former
`docs/CODE_AUDIT.md` was checked against the implementation, public contracts,
tests, examples, packaging, and HIL tooling. The already-applied Section 1 work
was revalidated by the complete regression suite.

The review favored the smallest change that preserves the owner-polled design,
transport injection, bounded work, and existing public compatibility surface.
Proposals that would create duplicate APIs, change intentional semantics, or
claim hardware coverage that the repository cannot provide were rejected and
are called out below.

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
| 2.12 owner path lacks clock/GPIO/offline knobs | Mostly incorrect; documentation clarified | Owner health already timestamps from `poll(nowMs)`, and owner reads intentionally use CONFIG OS polling rather than owning GPIO. Documented the fixed passive threshold of five for `DriverConfig`; no unused policy fields or framework hooks were added. |
| 2.13 portability and packaging claims are too broad | Partly valid | Lowered the framework-neutral core, CMake propagation, native tests, and packed-core compile gate to C++11. Added ESP-IDF component exclusions for repository-only CI, docs, tests, tools, and scripts. Kept ESP32-S2/S3 and `espressif32` target scope because those are explicit project requirements. Used `files.exclude`; the [Component Manager manifest rules](https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html#files) define `files.include` as a re-inclusion filter, not an allow-list. |
| 2.14 `setThresholds()` cannot program ready mode | Intentional API separation | Ordinary comparator thresholds require `high > low`; the atomic `enableConversionReadyPin()`/profile path owns the special ready encoding. No duplicate or ambiguous setter mode was added. |
| 2.15 legacy staged start methods discard their local token | Compatibility design, with documentation gap | Poll results already expose the token and examples consume `result.token`. Adding overloads would expand a legacy surface without a new capability. Doxygen now states exact start preconditions and results. |
| 2.16 idle specialized pollers return success | Valid | `pollSingleShot()` and `pollApplyConfig()` now return `RESULT_NOT_AVAILABLE`, `done=true`, and perform zero callbacks when idle. Unbound and wrong-operation behavior remains distinct. |
| 2.17 owner start preconditions are inconsistent | Mostly intentional; one real observer bug | Initialization/recovery intentionally require a binding; apply/read/shutdown require successful initialization. Those contracts are now explicit. `getAppliedProfile()` now leaves its output untouched when unbound. |
| 2.18 compatibility fields and aliases appear unused internally | Not a current defect | `strictInitVerify`, register aliases, and snapshot operation tokens are retained compatibility/diagnostic surfaces. Deprecating or removing them would create warnings or an unnecessary breaking change. Any removal belongs in an explicit major-version migration. |
| 2.19 `UINT32_MAX` is both timestamp and sentinel | Valid | Added an explicit private timestamp-valid flag and removed the sentinel encoding. A real start at `UINT32_MAX`, wraparound readiness, no-clock service arming, rollback, and unbind behavior are covered. |
| 2.20 generated version metadata is not reproducible | Valid | Generated-header fallbacks are deterministic `"unknown"` values instead of `__DATE__`/`__TIME__`. PlatformIO metadata defaults to the same deterministic values and honors `SOURCE_DATE_EPOCH` in UTC when supplied. Public macro and constant names remain compatible. |
| 2.21 register constants can drift from enums | Valid | Added compile-time assertions for register pointers, masks/shifts, MUX, PGA including aliases, data rate, mode, comparator queue, OS, and whole-register masks. No runtime or API cost was introduced. |
| 2.22 native fake is too permissive | Partly valid; full virtual ADC proposal rejected | The fake now records and can enforce I2C addresses, rejects conversion-register writes, and can model read-side elapsed time. Added direct coverage for timed-out job state and the new timing/address cases. Ambiguous failed writes were already modeled. A global virtual conversion engine would conflict with explicit owner-time tests and add substantial test complexity without addressing the reported production defects. |
| 2.23 HIL runner and CLI checker contain dead/unsafe paths | Valid except formatter subproposal | Removed the unused `failure_tokens` field and unreachable `UNKNOWN` result class, retaining `--fail-on-unknown` as a compatibility alias for evidence-required failure. Replaced the raw CONFIG write with a threshold write, and moved a complete nominal profile restoration after optional benchmarks. The CLI checker now inspects `processCommand()` dispatch predicates rather than help text. A new clang-format gate was not added: there is no pinned formatter/baselined tracked-file set, and forcing a repository-wide reformat is unrelated to these functional fixes. |
| 2.24 owner-safe lifecycle lacks automated hardware commands | Partly valid; proposed parallel CLI rejected | The owner-safe example already exercises real owner initialization, polling, token consumption, and reads. It now demonstrates bounded `startRecover()` handling with at most two attempts instead of latching the first runtime failure forever. A second command protocol was not mixed into the production ownership example, and it still could not synthesize physical transport faults or validate an unconfigured ALERT/RDY pin. Those remain dated hardware-validation items. |

## Resulting changes

- Corrected configuration trust, health accounting, readiness scheduling,
  shutdown state, timestamp representation, and compatibility poll semantics.
- Added compile-time register-contract checks and 14 focused native regressions;
  the suite increased from 178 to 192 tests.
- Made the core C++11-consumable without lowering the C++17 example builds.
- Made generated build metadata reproducible and reduced ESP-IDF package scope.
- Hardened fake transport, CLI dispatch checking, HIL classification, raw-write
  safety, final profile restoration, and the owner-safe example recovery path.
- Updated Doxygen, README, IDF guidance, changelog, and repository engineering
  rules to match the verified behavior.

## Verification

The final change was checked with:

- native PlatformIO tests: 192/192 passed under C++11;
- host `g++ -std=c++11 -Wall -Wextra -Werror` core compile;
- Arduino builds for diagnostic and owner-safe examples on ESP32-S2 and ESP32-S3;
- core timing/framework, CLI contract, ESP-IDF example, and version checks;
- HIL parser self-test and full two-address benchmark-plan dry run;
- PlatformIO package creation, unpack, generated-header presence, and packed-core
  C++11 compile;
- ESP-IDF manifest YAML parse and Doxygen warning-enforced generation.

No new physical hardware validation was performed, so this report makes no new
electrical, timing, analog-accuracy, or fault-injection claims.
