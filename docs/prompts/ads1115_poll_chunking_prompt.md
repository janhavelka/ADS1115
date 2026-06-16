# Prompt: ADS1115 Poll-Chunked I2C Execution

Target repo: https://github.com/janhavelka/ADS1115

## Goal

Add or document a simple poll-chunked execution path for ADS1115 so callers can
choose how many chip instructions are performed per poll. The default must work
with `maxInstructions = 1`; higher budgets may be used by diagnostics or setup
code when explicitly requested.

## Companion prompt boundary

The companion audit prompt owns framework neutrality, status/error behavior,
dirty/cache policy, blocking-convenience API classification, and raw output
requirements. This prompt owns only execution sequencing, instruction
accounting, delay gates, and budget tests.

## Required subagents

Before editing, spawn subagents:

- one explorer to map ADS1115 register operations and transfer counts,
- one explorer to map native tests and missing transaction-budget coverage,
- worker subagents only if they own disjoint files and are told not to revert
  unrelated edits.

## Common naming and semantics

Use these names unless the existing library has a clearly better local pattern:

- `startSingleShot(...)` starts an async conversion job and performs no I2C.
- `startApplyConfigJob(...)` starts staged config/register writes.
- `pollSingleShot(uint32_t nowMs, uint8_t maxInstructions)` advances sampling.
- `pollApplyConfig(uint32_t nowMs, uint8_t maxInstructions)` advances config.
- `cancelJob()`, `jobActive()`, `jobState()`, and `lastJobStatus()` expose state.
- `maxInstructions` counts transport callbacks only. An atomic register
  `writeRead` counts as one instruction even though it writes the pointer and
  reads data.
- Delay/readiness gates return waiting with `instructionsUsed = 0`.
- Clamp `maxInstructions` to a small fixed maximum. Do not add a scheduler
  framework or dynamic queues.

## ADS1115 chip sequencing

Atomic one-instruction operations:

- Read a 16-bit register with pointer write plus repeated-start read.
- Write a 16-bit register.
- Read conversion register.
- Read config register for OS/ready bit.
Do not split the pointer write and repeated-start read across polls.

Chunked jobs:

- Single-shot sample:
  1. write config with OS/start bit,
  2. wait `getConversionTimeMs()` for the selected SPS or ALERT/RDY,
  3. optionally poll config ready bit, one read per poll,
  4. read conversion register,
  5. convert only after raw value is captured.
- Config apply:
  1. threshold low write if needed,
  2. threshold high write if needed,
  3. config write,
  4. optional config readback.
- Recovery/reapply is the same as config apply and must expose partial-failure
  dirty state.

Multiple instructions per poll:

- Allow config apply to run more than one register write per poll when
  `maxInstructions > 1`.
- Allow ready-read plus conversion-read in one poll only when ready is already
  proven in that poll and budget remains.
- Keep `readBlocking()` as a convenience wrapper, not the TunnelMonitor model.
- Continuous latest-register reads can be immediate, but they are not proof of a
  fresh sample. Freshness needs a data-rate interval or ALERT/RDY evidence.

## Tests

- With `maxInstructions = 1`, prove one transport callback per poll call.
- With `maxInstructions = 2` or `3`, prove progress is faster but never exceeds
  the budget.
- Test conversion wait gates and transport-failure propagation through the
  chunked state machine.
- Preserve raw `int16_t` output and existing float helpers.
