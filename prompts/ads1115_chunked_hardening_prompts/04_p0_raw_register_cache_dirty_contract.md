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
