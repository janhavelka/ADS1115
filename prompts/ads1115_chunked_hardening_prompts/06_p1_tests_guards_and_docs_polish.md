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
