# Retained branch review

Reviewed on 2026-10-23 against `main` at
`381236ebfed813cd7a9aca34520ce031d8a0ff6b`, after fetching `origin` and confirming
that `main == origin/main` and the worktree was clean. This is the newest active
branch; the findings describe that source snapshot, before this documentation
commit. No old branch was merged, changed, or deleted.

## Decision

No important ADS1115 implementation was found stranded on an unmerged branch.
The only branch with commits outside `main` contains an exploration report and
implementation prompts, not a different driver, tests, or port. Its actionable
software findings have been implemented or superseded. Hardware qualification
remains open in [OPEN_ITEMS.md](OPEN_ITEMS.md); merging the report cannot supply
that evidence.

**Recommendation:** retire the exploration branch as an active work queue after
preserving its tip in a durable archive reference. The four other retained tips
below are already ancestors of `main` and are also retirement candidates. This
is a recommendation, not authorization or a record of branch deletion.

## Exact branch inventory

Counts are `git rev-list --left-right --count <main>...<tip>`: commits only on
the reviewed main, then commits only on the retained tip. All remote branches
were inspected; `origin/HEAD` is a symbolic alias, not another candidate.

| Retained `origin/` branch | Full tip SHA | Main-only / branch-only | Disposition |
| --- | --- | --- | --- |
| `exploration/ads1115-industry-standard` | `d6f163534d59ad8bdd4b597bfd7f64a93615ac94` | 88 / 1 | Unmerged documentation; findings reviewed below. |
| `feature/idf-port` | `4292bc9cd7c2e3639eaca2f92379f3992348a6a1` | 84 / 0 | Already contained; no port implementation to recover. |
| `hardening/ads1115-industry-readiness` | `65f6fdcc5c7b1d4da2d94eec5e4393614598e3f7` | 88 / 0 | Already contained. |
| `hardening/ads1115-industry-standard-p0` | `49e8df9e95a0242758079cb2133c1c8028051668` | 55 / 0 | Already contained. |
| `hardening/tunnelmonitor-suitability-reaudit` | `e84930f1cba3d75d8818eaaae83a08156cff7c7c` | 39 / 0 | Already contained; local branch has the same tip. |

The one unique commit is
`d6f163534d59ad8bdd4b597bfd7f64a93615ac94`. Its long subject begins
`docs: add integration examples, CI evidence, and HIL validation matrix`;
the actual patch changes `AGENTS.md` and adds
`docs/ADS1115_INDUSTRY_STANDARD_EXPLORATION.md`, nine sequenced prompt files,
and their consolidated prompt document. It changes **no** `src/`, `include/`,
`test/`, example, or workflow file. `git cherry <main> <tip>` reports `+`,
which establishes a unique patch, not a missing implementation.

## Historical findings reconciled with current source

IDs below refer to `docs/ADS1115_INDUSTRY_STANDARD_EXPLORATION.md` at the exact
exploration SHA above. Paths and line numbers in the evidence column refer to
the reviewed main snapshot; symbols/test names remain useful after line drift.

| Old finding | Current assessment | Evidence on main |
| --- | --- | --- |
| H1: failed initialization hides partial writes | Implemented. Initialization keeps uncertain hardware state and original failure; successful verified recovery clears it. | `src/ADS1115.cpp:1106` (`begin`, owner engine); `test/test_basic.cpp:859`, `:915`, `:962`, `:988`, `:1015`, `:1060` cover readback, every write position, and later probe failure. |
| H2: hardware/readiness overclaim | Claim boundary repaired; broader hardware evidence still open. This is not an unmerged code fix. | `docs/OPEN_ITEMS.md`; hardware plan/result template; README separates software contracts from board validation. |
| H3: overloaded BUSY and generic readback errors | Implemented and partly superseded. Explicit unsupported/readback/dirty/job errors exist; passive health now permits owner-authorized I/O, so reinstating the old proposed blanket OFFLINE gate would conflict with the current owner contract. | `include/ADS1115/Status.h:28`; `src/ADS1115.cpp:884`; tests `test_status_taxonomy_additions_are_append_only`, `test_passive_offline_diagnostic_allows_due_conversion_service`, `test_owner_safe_health_is_passive_and_recovery_transport_is_not_offline_gated`. |
| H4: raw writes silently desynchronize cache | Implemented. Raw writes mark uncertainty; typed sampling is gated until verified resynchronization. Definite address NACK has a narrower no-effect contract. | `src/ADS1115.cpp:2571` (`writeRegister16`); `test/test_basic.cpp:3785`, `:3809`, `:3826`, `:3921`, `:5348`, `:6243`. |
| M1: bool readiness hides errors | Implemented without removing compatibility API. `conversionReady(bool&)` and `readConversionReady(bool&)` return Status; bool-only wrapper remains a documented convenience. | `include/ADS1115/ADS1115.h:551`, `:556`; `test/test_basic.cpp:1684`. |
| M2: tick loses immediate I/O error | Implemented. Status-returning `service(nowMs)` exists; owner `poll()` is the production path. | `include/ADS1115/ADS1115.h:401`; `test/test_basic.cpp:1705`, `:2546`. |
| M3: optional clock underspecified | Implemented. Direct calls, explicit owner/service time, missing clock, and blocking-clock requirements have distinct documented contracts. | `include/ADS1115/Config.h:275`; `test/test_basic.cpp:1746`, `:1778`, `:4165`; README timing/compatibility tables. |
| M4: blocking readiness floods transport | Implemented through owner deadlines, timeout partitioning, conversion-rate rechecks, and stalled-clock reconciliation. | `src/ADS1115.cpp:1702`; tests `test_read_blocking_success_uses_cooperative_yield_cadence`, `test_owner_safe_multi_callback_poll_partitions_remaining_deadline_budget`, `test_owner_ready_poll_retry_is_data_rate_bounded`, `test_read_blocking_stalled_clock_enters_bus_silent_reconciliation`. |
| M5: IDF NACK mapping | Resolved as an explicit backend limitation. Unknown combined-transfer phase is deliberately not invented as address/data NACK; native error detail survives. Electrical cases remain qualification work. | `examples/esp_idf/basic/main/main.cpp:225`; `docs/IDF_PORT.md:108` error mapping table. |
| M6: native edge coverage | Relevant tests implemented, not merely renamed. | `test/test_basic.cpp:621`, `:639`, `:730` validation; `:881`, `:898` readback; `:3063`, `:3118` signed/scaling boundaries; `:3202` and later setter/dirty tests; `:6276` all legal addresses. |
| L1: framework-specific core commentary | Superseded by framework-neutral owner contracts; no FreeRTOS runtime dependency in core. | Current core and `tools/check_core_timing_guard.py`. |
| L2: differential wording | Clarified: two differential or four single-ended measurements; all eight MUX encodings, including four differential selections. | `README.md:56`; do not describe the multiplexed ADC as simultaneous sampling. |
| L3: narrow static guard | Implemented. Guard includes delay, framework headers/types, allocation and logging patterns. This remains a lexical guard, not whole-program proof. | `tools/check_core_timing_guard.py:12` and subsequent pattern sets. |

The old prompts' proposed implementation branch and historical test totals are
not current work orders or current verification results. The current native IDF
example and `.github/workflows/ci.yml` also already provide real IDF builds for
both ESP32-S2 and ESP32-S3.

## Work still needed

**P1 for a production qualification claim: execute the existing evidence queue.**
Use [OPEN_ITEMS.md](OPEN_ITEMS.md) as the single queue: current Arduino stack
requalification, final-board analog/strap validation, fault and recovery paths,
shared-bus scheduling/endurance, native-IDF hardware, and comparator/ALERT only
for enabled product profiles. Acceptance requires exact source/build identity,
fixture and wiring, observed statuses and timing, raw artifact location/hash,
and explicit unrun cases using the existing result template. Old green builds,
parser checks, and fake tests cannot close these items.

No additional branch-only software change is recommended from this review. A
new defect discovered during qualification should receive a focused current-main
fix and regression test; do not reopen the complete old prompt sequence.

## Reproduce and complete the retain/retire workflow

Run from the repository root. These comparison commands are read-only; use the
recorded main SHA so future documentation commits do not change the comparison.

```powershell
git status --short --branch
git for-each-ref --format="%(refname:short) %(objectname)" refs/heads refs/remotes/origin
git rev-list --left-right --count 381236ebfed813cd7a9aca34520ce031d8a0ff6b...origin/exploration/ads1115-industry-standard
git cherry 381236ebfed813cd7a9aca34520ce031d8a0ff6b origin/exploration/ads1115-industry-standard
git show --stat d6f163534d59ad8bdd4b597bfd7f64a93615ac94
git show d6f163534d59ad8bdd4b597bfd7f64a93615ac94:docs/ADS1115_INDUSTRY_STANDARD_EXPLORATION.md
git merge-base --is-ancestor origin/feature/idf-port 381236ebfed813cd7a9aca34520ce031d8a0ff6b
```

1. Refresh refs before any later retirement decision and repeat the comparisons
   for every tip in the inventory. A moved tip needs a new review.
2. Preserve any non-ancestor tip with a named archive tag/reference and record
   its full SHA; verify that archive is available remotely before branch removal.
3. Keep open hardware work in the existing queue, independent of branch lifetime.
4. Retire obsolete branch names only as a separate explicit maintenance action.
   No merge or cherry-pick is needed to recover functionality identified here.

## Verification and future CI

Actually run for this review: ancestry/count/cherry/stat inspection; semantic
source, test, documentation and workflow review; the three commands below all
passed. No native/firmware build, physical test, or remote CI run is claimed.

```powershell
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
git diff --check
```

Final documentation whitespace, local Markdown links, and every full commit SHA
were checked and passed. For a future substantial implementation change, reuse CI: native tests
(`.\scripts\pio.cmd test -e native` on Windows), the four Arduino environments
`esp32s3dev`, `esp32s2dev`, `owner_safe_s3`, `owner_safe_s2`, Doxygen, version
metadata, HIL parser/dry-run, package validation, and both native IDF targets.
CI pins PlatformIO 6.1.19 and the digest-pinned ESP-IDF 5.3.5 image. Verify the
exact final implementation commit's terminal CI result; this documentation-only
review does not dispatch remote CI.
