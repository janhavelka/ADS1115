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
