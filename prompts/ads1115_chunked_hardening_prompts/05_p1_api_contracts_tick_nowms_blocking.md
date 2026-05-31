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
