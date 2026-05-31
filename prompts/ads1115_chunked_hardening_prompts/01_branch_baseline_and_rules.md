# Prompt 01 — ADS1115 Branch Baseline, Rules, and Plan Lock

You are working in the ADS1115 repository.

This is a chunked hardening effort based on the latest ADS1115 industry-standard exploration report. The architecture is already strong; do not redesign the library. We will proceed through multiple prompts, one bounded chunk at a time. After every chunk, commit and sync.

## Goal of this chunk

Prepare the implementation branch and lock the working rules. Do not implement core changes yet except documentation/rules needed for the workflow.

## Required behavior

Spawn subagents:
- `repo-state-agent`
- `audit-gap-agent`
- `datasheet-contract-agent`
- `implementation-planner-agent`
- `final-review-agent`

Each subagent must provide concise, code-grounded findings.

## Start-up

Run:

```bash
git status --short
git branch --show-current
git log --oneline -5
```

If the worktree is dirty, stop and report the dirty files. Do not overwrite anything.

If currently on an exploration branch, switch back to the latest hardening implementation base branch if it exists:

```bash
git checkout hardening/ads1115-industry-readiness
git pull --ff-only
```

If that branch does not exist or is not the right branch, stop and report available branches. Do not guess.

Create a new implementation branch from the correct hardening baseline:

```bash
git checkout -b hardening/ads1115-industry-standard-p0
```

If this branch already exists, continue only if it is clearly the intended active branch.

## AGENTS.md update

Update `AGENTS.md` so future agents must obey:

- Work chunk-by-chunk; no broad refactor.
- Preserve framework-neutral core in `include/` and `src/`.
- No Arduino/Wire/ESP-IDF/FreeRTOS/logging/global bus/pin/task/framework delay dependencies in core.
- Core I2C stays injected and non-owning.
- Public fallible APIs return meaningful `Status`; do not hide transport errors behind bool-only/void APIs when changing or adding production APIs.
- Do not reorder existing status enum values unless compatibility impact is explicitly documented. Prefer appending new status codes.
- ADS1115 has no chip-ID register. Strict init/read-back is plausibility/read-back only.
- Multi-register writes can partially reach hardware. Dirty/partial hardware-state diagnostics must be explicit.
- Raw diagnostic register writes must either update cache safely or mark cache/hardware dirty.
- Public APIs are not ISR-safe and instances are not internally thread-safe unless explicitly proven.
- Hardware validation claims require dated logs/captures.
- CI/build claims require actual command output or CI config evidence.
- Each prompt must end with a commit and push/sync.

## Create implementation tracking report

Create or update:

```text
docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md
```

Initialize it with:
- branch name;
- starting commit;
- source audit report path;
- chunk plan;
- rules;
- table for prompt results;
- final report placeholder.

Do not mark any implementation complete yet.

## Validation

Run:

```bash
python tools/check_core_timing_guard.py
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
```

If PlatformIO is unavailable, record that exactly.

## Commit and sync

Commit only rule/report changes:

```bash
git add AGENTS.md docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md
git commit -m "docs: prepare ADS1115 industry-standard hardening plan"
git push -u origin hardening/ads1115-industry-standard-p0
```

Final response must include:
- current branch;
- changed files;
- checks run and exact results;
- commit hash;
- whether the branch was pushed;
- any blockers.
