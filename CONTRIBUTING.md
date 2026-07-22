# Contributing

Contributions should preserve the framework-neutral, transport-injected core
and the deterministic owner-safe contract described in [`README.md`](README.md).
Read [`AGENTS.md`](AGENTS.md) before changing implementation or public APIs.

## Workflow

1. Fork the repository and create a focused branch.
2. Make the smallest change that closes the current issue.
3. Add or update native fault-injection tests for changed behavior.
4. Update public documentation and the `[Unreleased]` changelog entry when the
   user-visible contract changes.
5. Run the checks relevant to the change and record only commands that actually
   completed.
6. Open a pull request with the behavior change, compatibility impact, and any
   unrun hardware/build cases stated explicitly.

## Engineering Rules

- Keep `include/` and `src/` free of Arduino, Wire, ESP-IDF, FreeRTOS, logging,
  global-bus, pin-ownership, and framework-delay dependencies.
- Keep I2C injected and non-owning. Applications own bus handles, locking,
  timeout policy, recovery, pins, and clock rate.
- Keep steady paths bounded and allocation-free; no unbounded waits, retries,
  queues, or buffers.
- Return meaningful `Status` from fallible public APIs. Preserve the original
  transport failure and explicit dirty-state diagnostics after partial or
  ambiguous hardware changes.
- Do not reorder existing status values. Append new values only when a new
  public error contract is necessary.
- Keep driver calls externally serialized and out of ISR context.
- Follow `.clang-format`, prefer `constexpr` constants, and use existing owners
  and helpers before adding abstractions.

## Verification

Run the complete host/guard set for core changes:

```bash
python tools/check_core_timing_guard.py
python scripts/generate_version.py check
python -m platformio test -e native
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
```

Build the affected examples and targets:

```bash
python -m platformio run -e esp32s3dev -e esp32s2dev
python -m platformio run -e owner_safe_s3 -e owner_safe_s2
```

Native ESP-IDF changes also require both configured IDF target builds. Hardware
claims require dated evidence from the current commit and exact fixture; parser
tests and dry-runs are not HIL evidence. Keep review documents concise: record
outcomes, failures, fixture identity, and artifact hashes instead of pasting
long serial transcripts.

## Commits And Pull Requests

Use clear, focused commits. Conventional prefixes such as `feat:`, `fix:`,
`docs:`, `refactor:`, `test:`, and `chore:` are preferred. A pull request should
not mix unrelated cleanup with a behavioral change.

Bug fixes, documentation, focused performance work with measurements, and
examples for common use cases are welcome. Discuss breaking APIs first. Heavy
dependencies, platform code in the core, and steady-state heap allocation are
outside the library contract.

Use the repository issue tracker for questions and proposed changes. Report
security issues privately as described in [`SECURITY.md`](SECURITY.md).
