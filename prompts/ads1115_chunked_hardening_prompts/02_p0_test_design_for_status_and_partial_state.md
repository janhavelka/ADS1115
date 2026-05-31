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
