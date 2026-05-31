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
