# Prompt 07 — Integration Examples, CI Evidence, and HIL Validation Matrix

Continue on the current ADS1115 hardening branch. Do not create a new branch.

This prompt improves integration evidence and prepares hardware validation. It should not require actual hardware unless available. Commit and sync at the end.

## Goal of this chunk

Make integration examples and validation evidence production-honest:

1. Arduino and ESP-IDF examples clearly labeled diagnostic vs production template.
2. ESP-IDF error mapping limitations documented.
3. CI/build evidence is clear.
4. HIL validation matrix and operator scripts are ready.

## Subagents

Spawn:
- `arduino-example-agent`
- `espidf-example-agent`
- `ci-agent`
- `hil-matrix-agent`
- `docs-agent`
- `final-review-agent`

## Arduino example review

Check:
- CLI is clearly diagnostic/bring-up.
- It does not pretend to be a production shared-bus manager.
- It reports active address, version, driver state, and relevant settings.
- It warns that `wreg` raw writes are diagnostic and affect dirty/cache state.
- It calls tick/service appropriately.
- It keeps I2C timeout policy explicit.
- It does not hide failures behind misleading output.

Update help/README if needed.

## ESP-IDF example review

Check:
- example uses native ESP-IDF APIs, not Arduino;
- bus ownership is in the example/adapter, not core;
- mutex locking is shown around I2C transactions;
- timeout is propagated;
- error mapping limitations are documented;
- local lack of `idf.py` is not reported as pass;
- CI container builds are correctly configured if present.

If possible, add a note/table:

| ESP-IDF error/fault | Current mapping | Precision limitation | Production recommendation |
| --- | --- | --- | --- |

Do not claim precise NACK data/address mapping unless you can prove it.

## CI/build evidence

Inspect `.github/workflows/*`.

Ensure CI covers, where possible:
- native tests;
- core guard;
- CLI guard;
- IDF example guard;
- Arduino ESP32-S2 build;
- Arduino ESP32-S3 build;
- pure ESP-IDF example build for ESP32-S2;
- pure ESP-IDF example build for ESP32-S3;
- package validation.

If CI cannot run something, document it as pending.

## HIL validation preparation

Create or update:

```text
docs/ADS1115_HARDWARE_VALIDATION_PLAN.md
```

Include concrete operator commands and expected evidence for:
- test identity: branch, commit, version, board, ADS1115 module, VDD, pull-ups, bus speed, ambient, wiring, instruments;
- address checks for 0x48, 0x49, 0x4A, 0x4B;
- wrong/missing address;
- mux raw/voltage for all MUX selections;
- gains for all PGA settings with safe input levels;
- data rates 8, 16, 32, 64, 128, 250, 475, 860 SPS;
- single-shot and continuous modes;
- blocking and service/tick paths;
- comparator traditional and window;
- latch, polarity, queue;
- ALERT/RDY conversion-ready capture at 8, 128, 860 SPS;
- stuck bus, unplug/replug, brownout/reset, recover;
- partial write/fault injection if possible;
- Arduino S2/S3 CLI;
- ESP-IDF S2/S3 example;
- 24-hour nominal soak and 2-hour 860 SPS stress.

Also create/update a results template:

```text
docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md
```

Do not fill it with fake results.

## Optional hardware scripts

If appropriate, add host-side helper scripts that only orchestrate serial commands and capture logs, for example:

```text
tools/hil_ads1115_capture.py
```

Requirements:
- script must not require hardcoded COM port;
- must print branch/commit/version command if possible;
- must save timestamped logs;
- must support dry-run or command-list printing;
- must not claim pass/fail without parsing actual output carefully.

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
```

If `idf.py` exists, run:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If not, record `idf.py` unavailable.

## Update report

Update `docs/ADS1115_INDUSTRY_STANDARD_IMPLEMENTATION_REPORT.md`.

## Commit and sync

```bash
git add examples .github tools README.md docs
git commit -m "docs: add ADS1115 integration and HIL validation plan"
git push
```

Final response must include:
- CI/build status;
- whether `idf.py` was available;
- HIL docs/scripts created;
- tests/checks and exact results;
- commit hash;
- pushed/synced status.
