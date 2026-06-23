# ADS1115 Production-Grade Release Readiness Follow-Up Prompt

You are working in `c:\Users\Honza\Documents\Projects\ADS1115` on the ADS1115
production embedded library. Follow `AGENTS.md` exactly. Preserve dirty user
changes. Keep changes simple, functional, robust, and scoped. Reuse existing
driver helpers, fake transport tests, HIL runner structure, diagnostic CLI
commands, and documentation templates wherever feasible.

## Goal

Close the remaining gaps that prevent an honest "production-grade / field-grade"
release claim, or document clearly why a gap remains blocked. Do not claim
production-grade readiness until all acceptance gates below are satisfied with
dated evidence.

The current README wording, "production-oriented/API-stable" with remaining
field validation pending, is still accurate until this prompt is completed.

## Current Known State To Verify First

Before editing, run or inspect:

```powershell
git status --short
python tools\run_i2c_hil.py --parser-test
python tools\run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted
pio test -e native
```

Known recent local work may already include:

- Active poll-job interleaving guard: normal public I2C/config APIs should return
  `Err::BUSY` with message `"Poll job active"` while a staged poll job is active.
- Native adversarial tests for poll budgets `0`, `1`, `2`, `3`, and `255`.
- HIL runner suite `--suite targeted`.

Do not duplicate already implemented work. Verify and update docs/reports to
match the current source.

## Subagent Split

Use subagents if helpful:

- Core Contracts Agent: staged-job contracts, dirty diagnostics, status codes,
  continuous-mode behavior, and native fake-transport tests.
- HIL/Hardware Agent: targeted HIL, full HIL, analog fixture, ALERT/RDY captures,
  fault injection, and evidence manifests.
- Docs/Release Agent: README, CHANGELOG, validation report/template, CI dry-run
  gates, and production-readiness wording.

## Code Contract Fixes

### 1. Allow Staged Config Apply In Normal Continuous Mode

Problem:

`startApplyConfigJob()` currently rejects `_conversionStarted`. In continuous
mode `_conversionStarted` can be true as normal background state, so staged
config apply is unavailable even though the apply job is the nonblocking path
needed by a shared bus owner.

Required design:

- Do not add a new public API or broad abstraction.
- Reuse existing `_config.mode`, `_conversionStarted`, and job state fields.
- `startApplyConfigJob()` must reject an active single-shot conversion, but must
  allow normal continuous-mode background conversion state.
- Keep existing `_jobActive` rejection.
- After a successful apply job:
  - If resulting mode is `Mode::CONTINUOUS`, set `_conversionStarted = true`,
    `_conversionReady = false`, and `_conversionStartMs` using `_nowMs()` when
    available, otherwise the supplied `nowMs`.
  - If resulting mode is `Mode::SINGLE_SHOT`, set `_conversionStarted = false`,
    `_conversionReady = false`, and `_conversionStartMs = 0`.

Concrete expected statuses:

- Active poll job: `Err::BUSY`, message `"Job already active"` or existing
  matching message.
- Active single-shot conversion: `Err::BUSY`,
  message `"Conversion already in progress"`.
- Normal continuous mode: `Err::IN_PROGRESS`,
  message `"Apply config job started"`.

Add native tests:

- `test_start_apply_config_job_in_continuous_mode_is_supported`
- `test_start_apply_config_job_rejects_active_single_shot_conversion`
- `test_poll_apply_config_continuous_mode_finishes_with_continuous_timing_state`

### 2. Make First Single-Shot Job Write Dirty Diagnostics Precise

Problem:

`_failJob()` currently marks `SINGLE_SHOT_WRITE_CONFIG` failures dirty
unconditionally once one instruction was attempted. For definite address absence
(`Err::I2C_NACK_ADDR`) this is misleading because no hardware write reached the
device.

Required design:

- For `_jobState == JobState::SINGLE_SHOT_WRITE_CONFIG`, use
  `_markHardwareConfigDirtyIfClean(status)`, the same conservative semantics
  used for first writes where hardware acceptance is uncertain.
- Keep dirty behavior for uncertain write failures:
  `Err::I2C_ERROR`, `Err::TIMEOUT`, `Err::I2C_NACK_DATA`,
  `Err::I2C_TIMEOUT`, and `Err::I2C_BUS`.
- Do not add a new `Err` code.

Add native tests:

- `test_poll_single_shot_first_write_address_nack_keeps_clean`
  - forced status: `Err::I2C_NACK_ADDR`, detail `-94`
  - expected: job `FAILED`, `instructionsUsed == 1`,
    `totalFailures == 1`, `consecutiveFailures == 1`,
    `hardwareConfigDirty() == false`
- `test_poll_single_shot_first_write_timeout_marks_dirty`
  - forced status: `Err::I2C_TIMEOUT`, detail `-95`
  - expected: job `FAILED`, `instructionsUsed == 1`,
    `hardwareConfigDirty() == true`,
    `hardwareConfigDirtyError().code == Err::I2C_TIMEOUT`,
    `hardwareConfigDirtyError().detail == -95`

### 3. Preserve Dirty Hardware Address Diagnostics

Problem:

If `begin()` fails after a write may have partially reached hardware,
diagnostics can report a dirty hardware state after `_config` has been reset to
defaults. That can make the dirty diagnostics point at the wrong address.

Required design:

- Add `SettingsSnapshot::hardwareConfigDirtyAddress`.
- Add private `_hardwareConfigDirtyAddress`.
- Set `_hardwareConfigDirtyAddress` from `_config.i2cAddress` inside
  `_markHardwareConfigDirty()` before `_config` can be reset.
- Clear it only when dirty hardware state is cleared by an explicit successful
  recovery/resync path.
- Do not change existing public dirty status semantics.

Suggested default when no dirty address is known:

```cpp
static constexpr uint8_t kInvalidDirtyAddress = 0x00;
```

Add native tests:

- `test_begin_partial_write_failure_preserves_dirty_address`
- `test_recover_success_clears_dirty_address`

### 4. Deprecate Bool-Only Conversion Ready API

Problem:

`bool conversionReady()` hides transport errors. Production callers need the
fallible API so NACKs, timeouts, bus faults, and degraded/offline transitions are
observable.

Required design:

- Keep `bool conversionReady()` for source compatibility.
- Mark it deprecated in Doxygen and, if compatible with supported toolchains,
  with a guarded `[[deprecated("Use readConversionReady(bool&)")]]` attribute.
- Route examples, HIL command handlers, and documentation to
  `readConversionReady(bool&)`.
- Do not remove the bool API in this chunk.

Add coverage:

- A native test proving `readConversionReady(bool&)` returns the injected
  transport error and updates health.
- A contract check or grep guard ensuring examples do not use
  `conversionReady()` except in compatibility-specific tests/docs.

### 5. Fix `readBlocking()` Fresh-Sample Semantics And Stalled-Clock Reporting

Problem:

In continuous mode, `readBlocking()` currently returns `readRaw()` immediately,
ignores `timeoutMs`, and still accepts `nowMs`. That behaves like "latest sample"
instead of "wait for a fresh sample." Separately, a stalled clock is currently
reported as a generic timeout after a very large same-tick loop guard.

Required design:

- Keep `readLatestRaw()` as the immediate/latest-sample API.
- In continuous mode, make `readBlocking()` wait for a fresh sample using
  existing `readConversionReady(bool&)` and deadline logic.
- Detect a non-advancing `nowMs` deterministically.
- Append a new status code only:

```cpp
Err::CLOCK_STALLED
```

- Do not reorder existing `Err` values. Append after
  `Err::HARDWARE_CONFIG_DIRTY`.
- Add:

```cpp
static constexpr uint16_t kMaxSameTickPolls = 1024U;
```

- Return `Err::CLOCK_STALLED` with `Status::detail == sameTickPolls` when the
  guard trips.
- Do not hide underlying transport failures behind `CLOCK_STALLED`.

Add native tests:

- `test_read_blocking_continuous_waits_for_fresh_ready_sample`
- `test_read_blocking_continuous_transport_error_is_preserved`
- `test_read_blocking_stalled_clock_returns_clock_stalled`
- `test_status_enum_clock_stalled_is_appended`

### 6. Update Poll-Job Documentation

Update README/Doxygen to state:

- While any poll-chunked job is active, normal public I2C/configuration APIs
  return `Err::BUSY`.
- `PollResult::instructionsUsed` counts transport callbacks only.
- `maxInstructions` is clamped to `3`.
- Passing `0` performs no transport work.
- Continuous-mode staged config apply is supported once item 1 is implemented.
- `conversionReady()` is compatibility-only; new code should use
  `readConversionReady(bool&)`.
- `readBlocking()` waits for a fresh sample. `readLatestRaw()` reads the latest
  conversion register value immediately.
- `Err::CLOCK_STALLED` means the supplied timebase did not advance after
  `kMaxSameTickPolls` same-tick polls.

## HIL Runner And Targeted HIL

### 7. Finish Targeted HIL On The Patched Firmware

The targeted HIL suite was prepared to avoid repeating long tests while still
probing feature families and malformed inputs.

Run after the ESP32-S2 application has actually rebooted out of the esptool
flasher stub:

```powershell
python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite targeted --timeout-s 8 --idle-s 0.5 --boot-settle-s 2 --stop-on-fail
```

Expected:

- `FAIL=0`.
- No target reboot, serial hang, or missing prompt.
- Valid feature commands return `Status: OK`, data output, or expected
  `IN_PROGRESS`.
- Active-job interleaving probes return `Status: BUSY`.
- Invalid commands return `Invalid`, `Usage:`, `Unknown command`, or equivalent
  visible parse rejection without increasing transport failure counters.

Record transcript and summary paths in a dated report.

### 8. Add A Release-Gate Unknown Policy

Problem:

The runner can return final verdict `UNKNOWN`, but process exit currently fails
only on `FAIL`. That is fine for exploratory HIL, but too weak for release gates.

Required design:

- Add `--fail-on-unknown`.
- When set, `main()` returns nonzero if any row result is `UNKNOWN`.
- Keep default behavior unchanged for exploratory runs.
- Add parser/dry-run coverage.
- Add CI dry-run command:

```bash
python tools/run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted
```

Do not pretend serial-only HIL proves analog accuracy. Use `--fail-on-unknown`
only for suites where `UNKNOWN` means the gate is incomplete, or document the
external evidence that resolves the unknowns.

### 9. Fix HIL Stress Summary And Analog Verdict Separation

Problems:

- The `stress_mix` validator can match an earlier progress/per-operation
  `fail=0` line instead of the final summary.
- Analog read rows intentionally return `UNKNOWN` when no external analog
  evidence exists, which can make the whole HIL verdict ambiguous even when the
  digital/API contract passed.

Required design:

- Replace the current stress failure regex with a summary-anchored parser, for
  example:

```python
STRESS_MIX_TOTAL_RE = re.compile(r"\bTotal:\s+ok=(\d+)\s+fail=(\d+)\b")
```

- Validate the final failure count from group 2.
- Add parser self-tests for:
  - progress lines with `fail=0` followed by final failure summary;
  - final `Total: ok=<n> fail=0`;
  - final `Total: ok=<n> fail=<n>`.
- Split final reporting into:
  - `contract_verdict`
  - `analog_evidence_verdict`
- Or add `--require-analog-evidence` if a smaller change fits the existing
  runner better.
- The default exploratory runner may still report analog evidence as `UNKNOWN`.
- Release gates must fail when analog evidence is required but missing.

## ESP-IDF Example And CLI Parity

### 10. Port Address Selection And Dirty Diagnostics To ESP-IDF

Problem:

The ESP-IDF example claims diagnostic parity but is effectively fixed to one
address and omits dirty-state fields from `settings`. Existing HIL targeted and
soak plans call `addr <address>`.

Required design:

- Keep the ESP-IDF example native IDF. Do not add Arduino, `Wire`, `String`,
  `Serial`, `TwoWire`, or Arduino compatibility facades.
- Reuse the Arduino CLI behavior and existing ESP-IDF transport helpers.
- Add or port these helper names if they fit the current file structure:

```cpp
activeI2cAddress
requestedI2cAddress
lastAddressSelectionStatus
isValidAds1115Address(uint8_t)
makeDriverConfig(uint8_t)
probeAddressRaw(uint8_t)
beginDriverAtAddress(uint8_t)
```

- Implement:

```text
addr <0x48..0x4B>
```

- Invalid addresses must be rejected without touching the active driver.
- Address changes must preserve the non-owning transport model.
- `settings` output must include the same production diagnostics as Arduino:
  - `Timebase available`
  - `Hardware/cache dirty`
  - dirty error code/detail/message
  - dirty hardware register/address context if available from item 3
- The main ESP-IDF loop should call `device.service(nowMs())`, not discard
  `tick()` failures. When verbose diagnostics are enabled, print/report service
  failures.

Add/extend checks:

- `python tools\check_idf_example_contract.py`
- HIL targeted dry-run must include ESP-IDF-supported commands only.

## Hardware Evidence Required Before Production-Grade Claim

### 11. Clean Commit HIL Evidence

The COM8 run is useful, but it reported firmware as dirty and current source is
newer than that tested firmware.

Before release:

- Start from a clean working tree.
- `git diff --exit-code` must pass before flashing.
- `version` output must identify the release commit and must not report dirty.
- Transcript commit must match `git rev-parse HEAD`.
- Store raw evidence or immutable links under:

```text
docs/evidence/hil/YYYY-MM-DD_COM8/
```

Use:

```text
MANIFEST.md
SHA256SUMS.txt
```

If raw logs are too large for git, store compressed logs externally and commit
the manifest with exact SHA-256 hashes, byte sizes, tool versions, and links.

### 12. Analog Accuracy Fixture

The existing COM8 run has `UNKNOWN` analog rows because there was no calibrated
source or DMM evidence. Complete the validation plan for all MUX/PGA/data-rate
families.

Suggested fixture acceptance constants, documented as fixture thresholds and
not as datasheet certification:

```text
ANALOG_ZERO_MAX_ABS_LSB = 8
ANALOG_VOLTAGE_MAX_REL_PCT = 0.5
ANALOG_VOLTAGE_MAX_ABS_MV = 2
RATE_CADENCE_MAX_REL_PCT = 10
```

Required measurements:

- AIN0-GND, AIN1-GND, AIN2-GND, AIN3-GND.
- AIN0-AIN1, AIN0-AIN3, AIN1-AIN3, AIN2-AIN3.
- Gain enums:
  - `Gain::FSR_6_144V`
  - `Gain::FSR_4_096V`
  - `Gain::FSR_2_048V`
  - `Gain::FSR_1_024V`
  - `Gain::FSR_0_512V`
  - `Gain::FSR_0_256V`
- Data-rate enums:
  - `DataRate::SPS_8`
  - `DataRate::SPS_16`
  - `DataRate::SPS_32`
  - `DataRate::SPS_64`
  - `DataRate::SPS_128`
  - `DataRate::SPS_250`
  - `DataRate::SPS_475`
  - `DataRate::SPS_860`

For each analog case, record:

- Applied source voltage.
- DMM voltage.
- ADS1115 raw code.
- Driver voltage.
- Gain, data rate, mux, mode.
- Error in mV and percent.
- Pass/fail against the fixture threshold.

### 13. ALERT/RDY And Comparator Electrical Validation

The library configures ALERT/RDY, but electrical behavior is not proven.

Required captures:

- ALERT/RDY conversion-ready at 8 SPS, 128 SPS, and 860 SPS.
- Traditional comparator.
- Window comparator.
- Active-low and active-high polarity.
- Non-latching and latching modes.
- Queue depths:
  - `ComparatorQueue::ASSERT_1`
  - `ComparatorQueue::ASSERT_2`
  - `ComparatorQueue::ASSERT_4`
  - `ComparatorQueue::DISABLE`

Suggested instrument thresholds:

```text
LOGIC_ANALYZER_MIN_SAMPLE_RATE = 10 MS/s
I2C_FAST_MODE_RISE_TIME_MAX_NS = 300
ALERT_RDY_CAPTURE_REQUIRED_AT_SPS = 8, 128, 860
```

Record pull-up values, voltage domain, capture sample rate, and screenshots or
logic analyzer files.

### 14. Hardware Fault Matrix

Run physical fault tests. Keep status mapping conservative.

Required cases:

- Missing device / wrong selected address.
- Unplug/replug ADS1115.
- Hold SDA low with current limiting.
- Hold SCL low with current limiting.
- ADS1115 brownout/reset with current-limited supply.
- Raw write dirty-state visibility and recovery.
- Partial-write injection if a safe I2C fault injector is available.

Expected status policy:

- `Err::DEVICE_NOT_FOUND` only when adapter proves definite
  `Err::I2C_NACK_ADDR`.
- Otherwise preserve original transport error:
  `Err::I2C_TIMEOUT`, `Err::I2C_BUS`, or `Err::I2C_ERROR`.
- Default `Config::offlineThreshold == 5`; verify `DriverState::OFFLINE` after
  5 consecutive tracked failures.
- After hardware is restored, `recover()` should return `Err::OK`,
  `state() == DriverState::READY`, and `consecutiveFailures() == 0`.
- Raw successful diagnostic write should set:
  - `hardwareConfigDirty() == true`
  - `hardwareConfigDirtyError().code == Err::HARDWARE_CONFIG_DIRTY`
  - `hardwareConfigDirtyError().detail == register pointer`
- Strict readback mismatch should set:
  - `Err::READBACK_MISMATCH`
  - `Status::detail == observed register value`

### 15. ESP-IDF Hardware Validation

Build coverage is not enough. Run pure ESP-IDF hardware checks on ESP32-S2 and
ESP32-S3.

Commands:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s2 build
idf.py -C examples/esp_idf/basic -p <PORT> flash monitor
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic -p <PORT> flash monitor
```

The ESP-IDF example must support `addr <0x48..0x4B>` before it is used for the
same multi-address HIL contract as Arduino. If that command is intentionally
deferred, document the limitation and do not claim ESP-IDF diagnostic parity.

Expected ESP-IDF command parity for diagnostics:

- `version`
- `scan`
- `addr <0x48..0x4B>`
- `probe`
- `settings`
- `drv`
- `read`, `readv`, `raw`, `voltage`
- `ch`, `diff`, `gain`, `rate`, `mode`
- `comp ...`
- `reg`, `wreg`, `config write`
- `job`, `job single`, `job apply`, `job poll [0..255]`, `job cancel`
- `shutdown`
- `recover`

Keep ESP-IDF error mapping conservative as documented in `docs/IDF_PORT.md`.

## Documentation And Release Metadata

Update these files only after evidence exists:

- `README.md`
- `CHANGELOG.md`
- `docs/README.md`
- `docs/IDF_PORT.md`
- `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md`
- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`
- New dated report under `docs/reports/`
- Evidence manifest under `docs/evidence/hil/YYYY-MM-DD_COM8/`

Required corrections:

- Remove stale "implemented: No" entries for active poll-job interleaving if the
  source and tests now prove it fixed.
- Document continuous-mode staged apply behavior after item 1.
- Document first-write dirty semantics after item 2.
- Document targeted HIL results after item 7.
- Keep "production-oriented/API-stable" wording until all hardware gates pass.
- Only then change release wording to "production-grade" or equivalent.

## Final Validation Commands

At minimum, after code/docs changes:

```powershell
python tools\run_i2c_hil.py --parser-test
python tools\run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted
python tools\check_cli_contract.py
python tools\check_idf_example_contract.py
python scripts\generate_version.py check
python tools\check_core_timing_guard.py
pio test -e native
pio run -e esp32s2dev
pio run -e esp32s3dev
pio pkg pack
git diff --check
```

If ESP-IDF is installed, also run:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s2 build
idf.py -C examples/esp_idf/basic set-target esp32s3 build
```

If hardware is available and rebooted into the app:

```powershell
python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite targeted --timeout-s 8 --idle-s 0.5 --boot-settle-s 2 --stop-on-fail
```

Do not rerun long soak/full HIL repeatedly while debugging parser or firmware
issues. Use targeted HIL first. Run full HIL and soak only from a clean release
candidate after code/docs stabilize.

## Release Decision Rule

The library can be called production-grade only when all are true:

- Native tests and static guards pass.
- Arduino ESP32-S2 and ESP32-S3 builds pass.
- ESP-IDF ESP32-S2 and ESP32-S3 builds pass, locally or with cited CI logs.
- Targeted HIL passes on the patched clean firmware.
- Full HIL and soak pass from a clean release commit.
- Analog fixture validation has no unresolved `UNKNOWN` rows.
- ALERT/RDY and comparator electrical captures are attached.
- Hardware fault matrix is complete or explicitly marked not applicable with a
  defensible reason.
- Evidence manifests include raw logs/captures or immutable links plus hashes.
- README/CHANGELOG/docs accurately reflect evidence and limitations.
- Final git status and commit/tag/release steps follow `AGENTS.md`.
