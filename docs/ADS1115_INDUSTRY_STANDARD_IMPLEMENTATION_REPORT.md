# ADS1115 Industry-Standard Implementation Report

Branch: `hardening/ads1115-industry-standard-p0`
Starting commit: `65f6fdcc5c7b1d4da2d94eec5e4393614598e3f7`
Source audit report: `exploration/ads1115-industry-standard:docs/ADS1115_INDUSTRY_STANDARD_EXPLORATION.md` at `d6f163534d59ad8bdd4b597bfd7f64a93615ac94`
Status: Planning and tracking initialized. No implementation fixes are marked complete yet.

## Scope Rules

- Proceed one bounded prompt at a time; no broad redesign.
- Preserve framework-neutral core code in `include/` and `src/`.
- Keep I2C transport injected and non-owning.
- Do not introduce Arduino, Wire, ESP-IDF, FreeRTOS, logging, global bus, pin
  ownership, task, or framework delay dependencies into the core.
- Public fallible production APIs must return meaningful `Status`.
- Append status enum values unless an explicit compatibility note justifies
  reordering.
- ADS1115 strict init/read-back is plausibility/read-back only; the device has no
  chip-ID register.
- Dirty/partial hardware-state diagnostics must be explicit for multi-register
  writes and raw diagnostic writes.
- Public APIs are not ISR-safe and instances are not internally thread-safe
  unless proven, documented, and tested.
- Hardware validation claims require dated logs or captures.
- CI/build claims require command output or CI configuration evidence.
- Every prompt ends with commit and push/sync.

## Chunk Plan

| Prompt | Planned scope | Implementation status |
| --- | --- | --- |
| 01 | Branch baseline, rules, and tracking report | Docs/rules only |
| 02 | P0 native test design for status and partial hardware state | Not started |
| 03 | P0 core status taxonomy and begin dirty-state implementation | Not started |
| 04 | P0 raw register cache/dirty contract | Not started |
| 05 | P1 API contracts for `tick()`, `nowMs`, and blocking latency | Not started |
| 06 | P1 tests, guards, and documentation polish | Not started |
| 07 | Integration examples, CI evidence, and HIL validation matrix | Not started |
| 08 | Final report and release-readiness review | Not started |

## Source Audit Summary

The exploration report classified the library as near industry-grade pending P0
fixes and validation. The highest-priority follow-up work is:

- Preserve visibility of partial hardware mutation when `begin()` fails after one
  or more writes.
- Tighten status taxonomy for offline, unsupported operation, strict read-back
  mismatch, and dirty/partial hardware state.
- Resolve or document public paths that can hide transport failure, especially
  bool-only readiness and void service paths.
- Define raw diagnostic register write cache/dirty semantics.
- Add native fault-injection coverage for begin-path partial writes, strict
  read-back branches, raw scaling, invalid config, and related status contracts.
- Produce dated HIL logs/captures before making field-grade hardware validation
  claims.

## Prompt Results

| Prompt | Branch/commit | Files changed | Validation | Result |
| --- | --- | --- | --- | --- |
| 01 | Pending commit | `AGENTS.md`, `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md` | Guards passed; native PlatformIO passed | Rules/report initialization only; no core implementation complete |

## Validation Log

Prompt 01 validation on `hardening/ads1115-industry-standard-p0`:

| Command | Result |
| --- | --- |
| `python tools/check_core_timing_guard.py` | `Core timing guard PASSED` |
| `python tools/check_cli_contract.py` | `CLI contract PASSED` |
| `python tools/check_idf_example_contract.py` | `IDF example contract PASSED` |
| `python scripts/generate_version.py check` | `Up to date: C:\Users\HonzovoSpectre\Documents\Projects\ADS1115\include\ADS1115\Version.h` |
| `python -m platformio test -e native` | `57 test cases: 57 succeeded in 00:00:02.147`; environment `native`, status `PASSED` |

## Final Report Placeholder

Final release-readiness conclusions will be written after all planned hardening
chunks complete and after the required validation evidence is available.
