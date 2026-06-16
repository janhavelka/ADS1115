# Prompt: ADS1115 I2C Library Hardening And TunnelMonitor Fit

Target repo: https://github.com/janhavelka/ADS1115

## Context

TunnelMonitor already has a single `I2cTask` owner. The owner uses fixed queues,
typed results, caller-owned deadlines, and normally performs at most one backend
transfer per owner `poll()`. ADS1115 library use was deferred because useful
high-level calls may hide several I2C transfers or readiness polls inside one
library call.

This prompt is for the ADS1115 library repo. Do not change TunnelMonitor
firmware unless a separate prompt explicitly asks for the adapter.

## Companion prompt boundary

This audit prompt owns correctness, stability, API cleanup, status/error
behavior, dirty/cache state, framework neutrality, raw output availability, and
nonblocking-vs-convenience API classification.

The companion `ads1115_poll_chunking_prompt.md` owns atomic I2C instruction
definitions, `maxInstructions`, per-poll sequencing, delay gates, job names, and
transaction-budget tests. Do not duplicate those details here.

## Subagents required

Before coding, spawn subagents and use their final notes in your plan:

- API explorer: inspect public APIs, private transfer helpers, timing loops,
  and hidden blocking or multi-transfer behavior by API name. Leave exact
  per-poll sequencing to the companion prompt.
- Test/package explorer: inspect native tests, examples, package metadata, and
  clean-consumer build behavior.
- If implementation is larger than a small patch, spawn worker subagents with
  disjoint file ownership. Tell workers they are not alone in the codebase and
  must not revert unrelated edits.

## Hard constraints

- Keep the core driver framework-neutral. No `Wire`, Arduino timing, FreeRTOS,
  heap-owning containers, or board policy in core driver headers/sources.
- No 64-bit timer change is required. Use `uint32_t` local timing with tested
  wrap-safe elapsed checks for short intervals.
- Raw/integer APIs are acceptable and preferred for TunnelMonitor-facing paths.
  Float helpers may remain as convenience APIs.
- Library health/status may remain, but TunnelMonitor will not treat it as the
  owner health authority. Do not expand driver health unless it reduces real
  failure ambiguity.
- Blocking convenience APIs may remain only if clearly marked unsuitable for
  TunnelMonitor steady paths.
- Keep timeouts caller-configurable. TunnelMonitor uses short per-transfer
  budgets such as 5 ms or 20 ms, not a hardcoded 50 ms assumption.

## What is already good

- Treat ADS1115 as one of the best baselines for the library family.
- Preserve callback transport, explicit `Err::OFFLINE`, dirty hardware
  diagnostics, `service(nowMs)`, raw/int read paths, and broad native tests.
- Preserve the split start/service/read style as the preferred shape for
  single-shot conversions.

## Required audit and fixes

1. Classify public methods as steady-path candidate, lifecycle/setup,
   diagnostic, or blocking convenience API.
2. Harden `begin()` and `recover()`: both call `_applyConfig()` and may perform
   multiple register writes plus optional readback in one call. Make partial
   failure behavior explicit and route staged sequencing to the companion
   prompt.
3. Keep `readBlocking()` and `readBlockingVoltage()` as convenience APIs only.
   Do not use them as the model for production polling.
4. Verify dirty-state behavior after partial `_applyConfig()` failure. If a
   write may have reached hardware but readback or a later write fails, expose a
   clear cache/hardware-uncertain signal.
5. Keep raw conversion helpers simple. Do not require voltage conversion before
   returning a sample.

## Required tests

- Tests proving `readBlocking()` remains bounded and is documented as
  non-steady-path.
- Timeout, NACK, bus error, and offline-latch propagation tests.
- Partial config apply failure and recovery tests.
- Existing native tests must still pass.

## Deliverables

- Code and tests in the ADS1115 repo.
- A short report listing steady-path candidates, convenience-only APIs, and any
  intentional compatibility breaks.
- No TunnelMonitor firmware changes in this prompt.
