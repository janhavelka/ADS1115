# ADS1115 Chunked Hardening Prompts



---

## 00_README_sequence.md

# ADS1115 Chunked Hardening Prompt Sequence

Use these prompts one by one in order. Each prompt is intended to be a bounded work chunk. The coding agent should spawn subagents, make focused changes, run the requested validation, commit, and sync after each chunk.

Do not skip straight to broad refactoring. The audit says the architecture is already strong; the remaining work is precision hardening, tests, validation evidence, and release honesty.

Recommended sequence:

1. `01_branch_baseline_and_rules.md`
2. `02_p0_test_design_for_status_and_partial_state.md`
3. `03_p0_core_status_and_begin_dirty_implementation.md`
4. `04_p0_raw_register_cache_dirty_contract.md`
5. `05_p1_api_contracts_tick_nowms_blocking.md`
6. `06_p1_tests_guards_and_docs_polish.md`
7. `07_integration_examples_ci_and_hil_matrix.md`
8. `08_final_report_release_readiness.md`

After every prompt:
- run the specified checks;
- update the per-prompt report section;
- commit with a clear message;
- push/sync the branch;
- stop and report if anything is ambiguous or unsafe.


---

## 01_branch_baseline_and_rules.md

# Prompt 01 — ADS1115 Branch Baseline, Rules, and Plan Lock

You are working in the ADS1115 repository.

This is a chunked hardening effort based on the latest ADS1115 industry-standard exploration report. The architecture is already strong; do not redesign the library. We will proceed through multiple prompts, one bounded chunk at a time. After every chunk, commit and sync.

## Goal of this chunk

Prepare the implementation branch and lock the working rules. Do not implement core changes yet except documentation/rules needed for the workflow.

## Required behavior

Spawn subagents:
- `repo-state-agent`
- `audit-gap-agent`
- `datasheet-contract-agent`
- `implementation-planner-agent`
- `final-review-agent`

Each subagent must provide concise, code-grounded findings.

## Start-up

Run:

```bash
git status --short
git branch --show-current
git log --oneline -5
```

If the worktree is dirty, stop and report the dirty files. Do not overwrite anything.

If currently on an exploration branch, switch back to the latest hardening implementation base branch if it exists:

```bash
git checkout hardening/ads1115-industry-readiness
git pull --ff-only
```

If that branch does not exist or is not the right branch, stop and report available branches. Do not guess.

Create a new implementation branch from the correct hardening baseline:

```bash
git checkout -b hardening/ads1115-industry-standard-p0
```

If this branch already exists, continue only if it is clearly the intended active branch.

## AGENTS.md update

Update `AGENTS.md` so future agents must obey:

- Work chunk-by-chunk; no broad refactor.
- Preserve framework-neutral core in `include/` and `src/`.
- No Arduino/Wire/ESP-IDF/FreeRTOS/logging/global bus/pin/task/framework delay dependencies in core.
- Core I2C stays injected and non-owning.
- Public fallible APIs return meaningful `Status`; do not hide transport errors behind bool-only/void APIs when changing or adding production APIs.
- Do not reorder existing status enum values unless compatibility impact is explicitly documented. Prefer appending new status codes.
- ADS1115 has no chip-ID register. Strict init/read-back is plausibility/read-back only.
- Multi-register writes can partially reach hardware. Dirty/partial hardware-state diagnostics must be explicit.
- Raw diagnostic register writes must either update cache safely or mark cache/hardware dirty.
- Public APIs are not ISR-safe and instances are not internally thread-safe unless explicitly proven.
- Hardware validation claims require dated logs/captures.
- CI/build claims require actual command output or CI config evidence.
- Each prompt must end with a commit and push/sync.

## Create implementation tracking report

Create or update:

```text
docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md
```

Initialize it with:
- branch name;
- starting commit;
- source audit report path;
- chunk plan;
- rules;
- table for prompt results;
- final report placeholder.

Do not mark any implementation complete yet.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
```

If PlatformIO is unavailable, record that exactly.

## Commit and sync

Commit only rule/report changes:

```bash
git add AGENTS.md docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md
git commit -m "docs: prepare ADS1115 industry-standard hardening plan"
git push -u origin hardening/ads1115-industry-standard-p0
```

Final response must include:
- current branch;
- changed files;
- checks run and exact results;
- commit hash;
- whether the branch was pushed;
- any blockers.


---

## 02_p0_test_design_for_status_and_partial_state.md

# Prompt 02 — P0 Test Design for Begin Partial-State and Status Taxonomy

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt is test-first. Add failing or pending tests that precisely define the P0 behavior before changing production implementation. Keep changes bounded. Commit and sync at the end.

## Goal of this chunk

Pin the expected behavior for:
1. failed `begin()` after partial hardware writes;
2. strict read-back mismatch status;
3. offline / unsupported-operation / partial-state status taxonomy;
4. raw register write cache/dirty behavior decision.

## Subagents

Spawn:
- `status-taxonomy-agent`
- `begin-partial-state-test-agent`
- `strict-readback-test-agent`
- `raw-register-contract-agent`
- `compatibility-review-agent`

## Status taxonomy design

Inspect `include/ADS1115/Status.h`.

Design status additions without breaking existing enum ordering unless there is no alternative. Prefer appending new enum members, for example:

```cpp
OFFLINE,
UNSUPPORTED_OPERATION,
READBACK_MISMATCH,
HARDWARE_CONFIG_DIRTY
```

Do not implement production code in this prompt unless needed only to make tests compile. If tests cannot compile without enum additions, add only the enum values and minimal string mapping, then leave production behavior failing.

## Tests to add

Add native fake-transport tests in `test/test_basic.cpp` or a new focused test file if the project structure supports it.

Add tests for:

### Begin partial writes

- Failure after first `_applyConfig()` write during `begin()` preserves a begin-failure dirty diagnostic.
- Failure after second `_applyConfig()` write during `begin()` preserves a begin-failure dirty diagnostic.
- Failure after third `_applyConfig()` write during `begin()` preserves original transport status.
- Failure during strict read-back after writes preserves a read-back mismatch or transport failure diagnostic.
- A later successful `begin()` or full `recover()` clears the begin dirty diagnostic only after full resync succeeds.

Do not require the driver to be initialized after failed `begin()`. The point is that diagnostic visibility must survive the uninitialized state.

### Status taxonomy

- Public I2C operation while offline returns `Err::OFFLINE` and does not touch the bus.
- `startConversion()` in continuous mode returns `Err::UNSUPPORTED_OPERATION`, not a generic busy.
- Strict read-back mismatch returns `Err::READBACK_MISMATCH`.
- Partial-state condition is distinguishable without parsing message strings.

### Raw register cache/dirty decision

First decide and write the expected contract in test names/comments:

Option A preferred unless code clearly supports safe cache update:
- raw writes to CONFIG / LO_THRESH / HI_THRESH are diagnostic writes and mark the cached configuration/hardware synchronization dirty or stale.

Add tests:
- successful raw CONFIG write marks dirty/stale or updates cache consistently.
- successful raw threshold write marks dirty/stale or updates cache consistently.
- raw write to invalid register remains rejected.
- raw write to conversion register remains rejected if it is read-only.

## Do not overreach

Do not rewrite the implementation in this prompt. It is acceptable if new tests fail. The purpose is to establish the contract.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python -m platformio test -e native
```

If tests fail because the new behavior is not implemented yet, that is acceptable. Record exact failing tests in the implementation report.

## Update report

Update:

```text
docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md
```

Add:
- test cases added;
- expected behavior;
- pass/fail status;
- implementation work required next.

## Commit and sync

Commit the tests and report:

```bash
git add test docs include/ADS1115/Status.h
git commit -m "test: define ADS1115 P0 partial-state and status contracts"
git push
```

Final response must include:
- tests added;
- exact command results;
- expected failures, if any;
- commit hash;
- pushed/synced status.


---

## 03_p0_core_status_and_begin_dirty_implementation.md

# Prompt 03 — P0 Core Implementation: Status Taxonomy and Failed-Begin Dirty Diagnostics

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt implements the P0 behavior defined by the previous tests. Keep the implementation focused. Commit and sync at the end.

## Goal of this chunk

Implement:
1. failed `begin()` dirty/partial hardware-state diagnostics;
2. precise status taxonomy for offline, unsupported operation, and strict read-back mismatch;
3. tests passing for these contracts.

## Subagents

Spawn:
- `core-status-agent`
- `begin-dirty-agent`
- `strict-readback-agent`
- `test-fault-agent`
- `api-compatibility-agent`
- `final-review-agent`

## Implementation requirements

### Status taxonomy

Append new status/error values without reordering existing ones unless there is an explicit compatibility note.

Add meaningful string names/messages for:
- `OFFLINE`
- `UNSUPPORTED_OPERATION`
- `READBACK_MISMATCH`
- `HARDWARE_CONFIG_DIRTY` or equivalent explicit partial-state code

Update all helper functions that stringify or classify status.

Avoid message-string parsing in tests. Tests should check status code and detail.

### Offline behavior

Where normal public I2C paths currently return a generic busy/offline-like status, return `Err::OFFLINE` without touching the bus.

Keep `recover()` as the explicit path allowed to access the bus while offline.

### Unsupported operation

When `startConversion()` is called in continuous mode, return `Err::UNSUPPORTED_OPERATION`, not generic `BUSY`.

Do not break the existing semantics where a true in-progress single-shot conversion remains distinguishable.

### Strict read-back mismatch

Strict read-back mismatch of CONFIG, LO_THRESH, or HI_THRESH must return `Err::READBACK_MISMATCH`.

Preserve observed register/field details in `Status::detail` or a documented diagnostic field.

Keep CONFIG OS/status bit masked during strict comparison.

### Failed begin partial-state diagnostics

Fix the path where `begin()` performs one or more writes and then fails, but clears dirty diagnostics because the driver remains uninitialized.

Acceptable design:
- keep `initialized == false`;
- preserve sticky diagnostic fields such as:
  - last begin error;
  - begin hardware config dirty;
  - begin dirty error;
  - last partial-apply status;
- expose them through existing diagnostics if possible, or add carefully named accessors / snapshot fields.

The application must be able to know:
- `begin()` failed;
- hardware may have been partially changed;
- original transport or read-back error;
- whether a later successful `begin()`/`recover()` cleared the condition.

Do not pretend the driver is initialized just to expose diagnostics.

### Dirty clearing

Clear dirty/begin-dirty state only after a successful full config apply/resync/read-back path, not merely after a probe.

## Tests

Run and update the tests from prompt 02 until they pass.

Also add small regression tests if implementation creates new edge cases.

## Documentation

Update README/Doxygen minimally to document:
- new status codes;
- failed begin partial-state diagnostics;
- strict read-back mismatch status;
- offline vs unsupported vs busy distinction.

Do not do broad documentation polish here.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

## Update report

Update:

```text
docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md
```

Add:
- implementation summary;
- public API/status changes;
- compatibility notes;
- tests run;
- exact results;
- remaining P0/P1 gaps.

## Commit and sync

```bash
git add include src test README.md docs
git commit -m "fix: expose ADS1115 begin partial-state and precise statuses"
git push
```

Final response must include:
- changed APIs/statuses;
- tests/checks and exact results;
- commit hash;
- pushed/synced status;
- any unresolved P0 issue.


---

## 04_p0_raw_register_cache_dirty_contract.md

# Prompt 04 — P0 Raw Register Write Cache/Dirty Contract

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt resolves the diagnostic raw-write contract. Keep it focused. Commit and sync at the end.

## Goal of this chunk

Make raw public register writes impossible to silently desynchronize software cache and hardware.

The audit found that `writeRegister16()` can write CONFIG / LO_THRESH / HI_THRESH without updating cache or marking dirty. That is unacceptable for production diagnostics.

## Subagents

Spawn:
- `raw-register-contract-agent`
- `cache-consistency-agent`
- `tests-agent`
- `docs-agent`
- `final-review-agent`

## Decide and implement one contract

Preferred contract:

> Raw register writes are diagnostic-only. Any successful raw write to CONFIG, LO_THRESH, or HI_THRESH marks the hardware/cache synchronization dirty or stale. Typed setters/readers/recover can clear that state only after a full verified resync.

Alternative acceptable contract only if simple and safe:

> Raw writes to CONFIG and thresholds update the typed cache exactly, with validation and masking, and mark dirty only if cache cannot be represented safely.

Do not leave behavior implicit.

## Implementation requirements

- `writeRegister16()` and any alias must reject read-only conversion register writes.
- Raw write to CONFIG must either update cache exactly or mark cache/hardware dirty.
- Raw write to LO_THRESH or HI_THRESH must either update threshold cache exactly or mark cache/hardware dirty.
- If raw write bypasses typed invariants, diagnostics must make that visible.
- Dirty reason should preserve that this came from a raw diagnostic write, not a transport failure.
- `getSettings()` must not falsely imply cache is synchronized after a raw write.
- `recover()` / full apply / successful strict resync must clear the raw-write dirty state only when synchronized.
- CLI help should continue to warn that `wreg` is diagnostic and may desync cached config, but now should mention that the driver marks this state.

## Tests

Ensure tests cover:

- raw CONFIG write dirty/stale behavior;
- raw LO_THRESH write dirty/stale behavior;
- raw HI_THRESH write dirty/stale behavior;
- invalid register rejection;
- conversion register write rejection;
- recovery/full apply clears raw dirty only after success;
- failed recover does not clear raw dirty.

## Docs

Update:
- README raw register diagnostics section;
- Doxygen for raw register write;
- CLI help if the command text is checked by guard scripts;
- implementation report.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

## Commit and sync

```bash
git add include src test README.md docs examples tools
git commit -m "fix: mark ADS1115 raw register writes as cache-dirty"
git push
```

Final response must include:
- selected raw-write contract;
- code/docs changed;
- tests/checks and exact results;
- commit hash;
- pushed/synced status.


---

## 05_p1_api_contracts_tick_nowms_blocking.md

# Prompt 05 — P1 API Contract Polish: conversionReady, tick/service, nowMs, and Blocking Bounds

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt handles public API clarity without broad redesign. Commit and sync at the end.

## Goal of this chunk

Resolve or sharply document:
1. bool-only `conversionReady()` hiding transport errors;
2. `tick()` performing I2C but returning void;
3. `nowMs` no-clock behavior outside blocking APIs;
4. `readBlocking()` polling cadence and latency documentation.

## Subagents

Spawn:
- `api-contract-agent`
- `timing-agent`
- `tests-agent`
- `docs-agent`
- `compatibility-agent`
- `final-review-agent`

## conversionReady contract

Keep existing `bool conversionReady()` for source compatibility if present.

Required:
- Mark it clearly as a convenience API.
- Document false means either "not ready" or "error"; production users should use status-returning readiness API.
- Ensure there is a status-returning readiness API, preferably already `readConversionReady(bool&)`.

Optional but desirable if API-compatible:
- Add a clearer alias:
  ```cpp
  Status conversionReady(bool& ready);
  ```
  implemented by delegating to `readConversionReady(bool&)`.

Tests:
- bool convenience returns false on transport error;
- status-returning API preserves exact transport/status error.

## tick/service contract

Keep existing `void tick(uint32_t nowMs)` for compatibility.

Required:
- Document that `tick()` may perform I2C when a single-shot conversion is pending.
- Document failures are visible through health state/counters/last error.

Optional if clean:
- Add:
  ```cpp
  Status service(uint32_t nowMs);
  ```
  where `tick()` calls and ignores status, or both share internal implementation.

Tests:
- tick/service I2C failure updates health and lastError;
- if `service()` is added, it returns the immediate status.

## nowMs no-clock contract

The audit found that `nowMs` affects health timestamps and direct nonblocking readiness, not only blocking reads.

Choose one:

Option A:
- Require `nowMs` in `begin()` for all managed conversion/health features.

Option B preferred if preserving lightweight use:
- Keep `nowMs` optional, but document degraded no-clock behavior:
  - blocking APIs require it;
  - health timestamps unavailable;
  - direct nonblocking readiness may not advance by elapsed time without either `nowMs` hook or `tick(nowMs)`;
  - ALERT/RDY GPIO path can still work if configured.

If Option B, expose a diagnostic/snapshot field such as `timebaseAvailable` if this can be done cleanly.

Tests:
- no-clock direct readiness behavior is pinned;
- blocking APIs still fail before starting conversion when no `nowMs`;
- diagnostics do not misleadingly present timestamp 0 as a real time.

## readBlocking polling cadence

Audit and improve only if low-risk.

Required:
- Document real worst-case: API timeout plus per-transaction timeout behavior.
- Ensure no unbounded same-tick spin.
- Prefer a sane poll cadence with cooperative yield if provided.
- Keep deterministic behavior and tests.

Optional:
- Cap poll count or compute remaining budget for readiness polling.
- Add tests with fake clock that advances slowly and verifies bounded transaction count.

## Datasheet-specific docs to keep accurate

Do not claim `readBlocking()` in continuous mode waits for a new sample unless implemented. Datasheet says continuous mode places each completed conversion into the conversion register and immediately starts another conversion; reading returns the current/latest register value.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

## Update report

Update `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` with:
- API additions/deprecations;
- compatibility notes;
- timing/nowMs contract;
- tests and results.

## Commit and sync

```bash
git add include src test README.md docs examples tools
git commit -m "docs: clarify ADS1115 readiness and service timing contracts"
git push
```

Final response must include:
- API behavior decisions;
- whether new APIs were added;
- tests/checks and exact results;
- commit hash;
- pushed/synced status.


---

## 06_p1_tests_guards_and_docs_polish.md

# Prompt 06 — P1 Test Expansion, Guard Scripts, and Documentation Honesty

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt expands edge coverage and guard scripts. Keep production code changes minimal; this is mostly tests/docs/tools. Commit and sync at the end.

## Goal of this chunk

Add missing edge tests and strengthen guards so the repo stays industry-standard after future edits.

## Subagents

Spawn:
- `test-gap-agent`
- `guard-script-agent`
- `docs-honesty-agent`
- `release-wording-agent`
- `final-review-agent`

## Native tests to add

Add focused tests for:

### Invalid configuration and enum boundaries

- invalid I2C address;
- invalid enum values if public setters accept raw/integer paths;
- invalid ALERT/RDY GPIO config if applicable;
- invalid threshold boundary values if relevant.

### Transport taxonomy on tracked paths

For normal tracked reads/writes, verify preservation of:
- `I2C_NACK_ADDR`;
- `I2C_NACK_DATA`;
- `I2C_TIMEOUT`;
- `I2C_BUS`;
- generic `I2C_ERROR`.

### Strict read-back branches

- low-threshold mismatch;
- high-threshold mismatch;
- config mismatch with OS bit masked correctly;
- read-back transport failure;
- strict recover success and mismatch behavior.

### Threshold and scaling boundaries

- `getThresholds()` sign reconstruction for positive, negative, `INT16_MIN`, `INT16_MAX`;
- `setThresholds()` boundaries;
- `rawToVoltage()` for all gains at representative raw values: `0`, `1`, `-1`, `INT16_MAX`, `INT16_MIN`;
- `getLsbVoltage()` for all gains;
- PGA alias behavior remains documented/tested.

### Setter rollback variants

- `setMux()`, `setGain()`, `setDataRate()`, `setMode()` rollback on write failure;
- comparator setters rollback on write failure;
- dirty flag preservation when a config-only setter fails while pre-existing dirty state exists.

## Guard script expansion

Expand `tools/check_core_timing_guard.py` or add a new guard to reject framework leakage in core:

Forbidden in `include/` and `src/`:
- `Arduino.h`
- `Wire.h`
- `TwoWire`
- `Serial`
- `String`
- `delay(`
- `delayMicroseconds`
- `millis`
- `micros`
- `yield(`
- ESP-IDF includes
- FreeRTOS includes
- logging macros such as `ESP_LOG`, `Serial.print`
- dynamic allocation patterns if project rules forbid them: `new `, `delete `, `std::vector`, `std::string`

Avoid false positives in comments only if practical. At minimum, keep the guard useful and documented.

## Documentation honesty

Update docs so they do not overclaim:

- Reword any `production-ready` claim to `production-oriented`, `API-stable candidate`, or `feature-complete pending hardware validation`, unless the repo has fresh HIL logs.
- Ensure README and CHANGELOG agree.
- Add or update validation-results matrix with empty/pending rows, not fake passes.
- Clarify address strap mapping and SDA/SCL pull-up sizing caveat from datasheet.
- Clarify differential wording: ADS1115 supports four differential MUX selections, while the high-level summary says two differential inputs/pairs.
- Keep ALERT/RDY 8 µs pulse caveat in docs.
- Keep PGA absolute input warning in docs.

## Version-sync check

If version files exist across `library.json`, `idf_component.yml`, `Doxyfile`, generated `Version.h`, and package metadata, add or update a script/check that verifies they agree.

Do not bump version unless explicitly appropriate; if you do, document why.

## Validation

Run:

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

Remove generated package artifacts after validation unless the repo intentionally tracks them.

## Update report

Update `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` with:
- tests added;
- guard changes;
- docs/release wording changes;
- exact command results.

## Commit and sync

```bash
git add include src test tools scripts README.md CHANGELOG.md docs Doxyfile library.json idf_component.yml
git commit -m "test: expand ADS1115 edge coverage and core guards"
git push
```

Final response must include:
- number of native tests before/after;
- guard script scope;
- docs wording corrected;
- tests/checks and exact results;
- commit hash;
- pushed/synced status.


---

## 07_integration_examples_ci_and_hil_matrix.md

# Prompt 07 — Integration Examples, CI Evidence, and HIL Validation Matrix

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt improves integration evidence and prepares hardware validation. It should not require actual hardware unless available. Commit and sync at the end.

## Goal of this chunk

Make integration examples and validation evidence production-honest:

1. Arduino and ESP-IDF examples clearly labeled diagnostic vs production template.
2. ESP-IDF error mapping limitations documented.
3. CI/build evidence is clear.
4. HIL validation matrix and operator scripts are ready.

## Subagents

Spawn:
- `arduino-example-agent`
- `espidf-example-agent`
- `ci-agent`
- `hil-matrix-agent`
- `docs-agent`
- `final-review-agent`

## Arduino example review

Check:
- CLI is clearly diagnostic/bring-up.
- It does not pretend to be a production shared-bus manager.
- It reports active address, version, driver state, and relevant settings.
- It warns that `wreg` raw writes are diagnostic and affect dirty/cache state.
- It calls tick/service appropriately.
- It keeps I2C timeout policy explicit.
- It does not hide failures behind misleading output.

Update help/README if needed.

## ESP-IDF example review

Check:
- example uses native ESP-IDF APIs, not Arduino;
- bus ownership is in the example/adapter, not core;
- mutex locking is shown around I2C transactions;
- timeout is propagated;
- error mapping limitations are documented;
- local lack of `idf.py` is not reported as pass;
- CI container builds are correctly configured if present.

If possible, add a note/table:

| ESP-IDF error/fault | Current mapping | Precision limitation | Production recommendation |
| --- | --- | --- | --- |

Do not claim precise NACK data/address mapping unless you can prove it.

## CI/build evidence

Inspect `.github/workflows/*`.

Ensure CI covers, where possible:
- native tests;
- core guard;
- CLI guard;
- IDF example guard;
- Arduino ESP32-S2 build;
- Arduino ESP32-S3 build;
- pure ESP-IDF example build for ESP32-S2;
- pure ESP-IDF example build for ESP32-S3;
- package validation.

If CI cannot run something, document it as pending.

## HIL validation preparation

Create or update:

```text
docs/ADS1115_HARDWARE_VALIDATION_PLAN.md
```

Include concrete operator commands and expected evidence for:
- test identity: branch, commit, version, board, ADS1115 module, VDD, pull-ups, bus speed, ambient, wiring, instruments;
- address checks for 0x48, 0x49, 0x4A, 0x4B;
- wrong/missing address;
- mux raw/voltage for all MUX selections;
- gains for all PGA settings with safe input levels;
- data rates 8, 16, 32, 64, 128, 250, 475, 860 SPS;
- single-shot and continuous modes;
- blocking and service/tick paths;
- comparator traditional and window;
- latch, polarity, queue;
- ALERT/RDY conversion-ready capture at 8, 128, 860 SPS;
- stuck bus, unplug/replug, brownout/reset, recover;
- partial write/fault injection if possible;
- Arduino S2/S3 CLI;
- ESP-IDF S2/S3 example;
- 24-hour nominal soak and 2-hour 860 SPS stress.

Also create/update a results template:

```text
docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md
```

Do not fill it with fake results.

## Optional hardware scripts

If appropriate, add host-side helper scripts that only orchestrate serial commands and capture logs, for example:

```text
tools/hil_ads1115_capture.py
```

Requirements:
- script must not require hardcoded COM port;
- must print branch/commit/version command if possible;
- must save timestamped logs;
- must support dry-run or command-list printing;
- must not claim pass/fail without parsing actual output carefully.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

If `idf.py` exists, run:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If not, record `idf.py` unavailable.

## Update report

Update `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md`.

## Commit and sync

```bash
git add examples .github tools README.md docs
git commit -m "docs: add ADS1115 integration and HIL validation plan"
git push
```

Final response must include:
- CI/build status;
- whether `idf.py` was available;
- HIL docs/scripts created;
- tests/checks and exact results;
- commit hash;
- pushed/synced status.


---

## 08_final_report_release_readiness.md

# Prompt 08 — Final Integration Report and Release Readiness Gate

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This is the final consolidation prompt for this chunked hardening pass. Do not add new features unless needed to fix a failing check or obvious documentation inconsistency. Commit and sync at the end.

## Goal of this chunk

Produce a comprehensive final report for all changes and remaining future work. Confirm whether the repo is ready to merge and whether it is ready to release.

## Subagents

Spawn:
- `diff-review-agent`
- `datasheet-contract-review-agent`
- `test-ci-review-agent`
- `docs-release-review-agent`
- `hardware-validation-review-agent`
- `final-verdict-agent`

## Review tasks

### 1. Full diff review

Run:

```bash
git status --short
git log --oneline --decorate -10
git diff --stat
git diff --name-only origin/main...HEAD || true
```

If `origin/main...HEAD` is not the right comparison, use the correct base and report it.

Check for:
- accidental broad refactors;
- accidental generated artifacts;
- changed public API without docs;
- framework leakage into core;
- overclaimed hardware/CI results.

### 2. Datasheet contract review

Verify again:
- no chip-ID claim;
- four ADS1115 addresses;
- four SE channels / differential MUX wording;
- PGA FSR versus analog input limits;
- register map/pointer behavior;
- continuous latest-register semantics;
- ALERT/RDY open-drain/pull-up and short pulse caveat;
- comparator latch clearing behavior;
- general-call reset not used silently.

### 3. Run full available validation

Run:

```bash
python --version
python -m platformio --version
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio pkg pack
```

Remove package artifacts after validation unless intentionally tracked.

If available:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If unavailable, record exactly.

### 4. Final report

Create/update:

```text
docs/ADS1115_INDUSTRY_STANDARD_FINAL_REPORT.md
```

Required sections:

```markdown
# ADS1115 Industry-Standard Hardening Final Report

Date:
Branch:
Starting commit:
Final commit:

## Executive Summary

## What Changed By Chunk

## Public API / Status Changes

## Compatibility Notes

## Datasheet Contract Review

## Tests Added/Changed

## Commands Run And Exact Results

## CI Coverage

## Hardware Validation Status

## Remaining Blockers

Separate:
- Must fix before merge
- Must validate before release
- Future industry-grade evidence
- Nice-to-have

## Release Wording Recommendation

State exactly what can be claimed and what must not be claimed.

## Merge Recommendation

Ready / Not ready / Ready with conditions.

## Release Recommendation

Ready / Not ready / Ready as pre-release only.

## Future Work Backlog

## Files Changed
```

## Release wording rules

Do not claim:
- "field-proven";
- "fully industry-grade";
- "production-ready";
- "all hardware validated";
unless the repo contains actual dated evidence.

Acceptable wording if checks pass but HIL remains incomplete:

> Production-oriented ADS1115 driver with framework-neutral core, injected I2C transport, explicit timing/error contracts, strong native fault tests, Arduino ESP32-S2/S3 build coverage, and prepared ESP-IDF/HIL validation paths. Hardware/fault validation remains required before field-grade claims.

## Optional final tag/version recommendation

If API/status changes are breaking or semi-breaking:
- recommend version bump strategy;
- do not tag unless explicitly asked.

## Commit and sync

```bash
git add docs README.md CHANGELOG.md include src test tools examples .github library.json idf_component.yml Doxyfile
git commit -m "docs: finalize ADS1115 industry-standard hardening report"
git push
```

If there are no changes to commit, report that clearly.

Final response must include:
- final branch and commit;
- full command results;
- final readiness verdict;
- exact remaining blockers;
- files changed;
- pushed/synced status;
- whether it is ready for the user to perform hardware validation.
