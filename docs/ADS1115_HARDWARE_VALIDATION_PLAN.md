# ADS1115 Hardware Validation Plan

This is the operator procedure for evidence that is still missing. It is not
validation evidence, and the repository does not retain a current-stack
physical qualification result.

## Acceptance Rule

Mark a case `Pass` only when a dated results record identifies the exact clean
firmware commit, hardware, wiring, instruments, command, observed result, and
evidence location. ADS1115 has no identity register, so a successful probe and
configuration readback prove only reachability and plausibility.

Do not commit full serial transcripts, generated runner summaries, firmware
dumps, or superseded fixture notes. Store required lab evidence in the approved
evidence system and record its stable reference in the dated result.

## Unfinished Gates

| Gate | Required coverage | Evidence needed |
| --- | --- | --- |
| Runtime identity | Clean firmware matching the intended commit, pioarduino `55.03.311`, Arduino-ESP32 `3.3.11`, ESP-IDF `v5.5.5`, and build timestamp | Build and startup identity record |
| Arduino physical HIL | ESP32-S2/S3 targeted and exhaustive plans; populated and expected-absent addresses | Dated result and external evidence reference |
| Address straps | Physical ADDR-to-GND/VDD/SDA/SCL setups (`0x48`-`0x4B`) | Wiring record/photo and observed address behavior |
| Calibrated analog | All eight MUX choices, six PGA ranges, and eight data rates using safe, measured sources | DMM/source readings, raw codes, converted values, tolerances |
| Timing and ALERT/RDY | Single-shot readiness and 8/128/860 SPS timing; conversion-ready pulses | Timestamp data and scope/logic captures |
| Comparator electrical | Traditional/window, polarity, latch, and queue depth | Applied stimulus, thresholds, output levels, captures |
| Physical faults and recovery | Missing device, unplug/replug, stuck SDA/SCL, ADS1115 brownout/reset, raw-write dirty state, partial/ambiguous transfer | Exact status/detail/message, dirty/trust state, recovery result |
| Shared-bus workload | External serialization, contention, bounded callback latency, cancellation/reconciliation, production task cadence | Compact integration timing/fault result |
| Native ESP-IDF hardware | ESP32-S2 and ESP32-S3 native examples; no Arduino compatibility layer | Compact build/flash/monitor outcomes |
| Final-workload endurance | Acceptance-duration nominal soak and worst-rate stress on the selected final board/workload, with limits chosen before the run | Duration, cycles/commands, latency, failures, resets, environment |
| Final-board acceptance | Actual product board supply, pull-ups, protection, source impedance, disconnect/saturation behavior, calibration | Schematic/setup identity and signed acceptance record |

## Record Identity Before Testing

Use `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`. At minimum record:

- start/end time, operator, branch, full commit, tag/version, and clean/dirty
  state;
- host OS and tool versions, firmware build timestamp, and exact build/flash
  commands;
- pioarduino, Arduino-ESP32, and ESP-IDF versions reported by the running
  firmware;
- ESP32 board/revision, ADS1115 module/revision, address strap, VDD, bus speed,
  pull-ups, wiring, ambient conditions, and instrument identifiers;
- externally serialized bus owner, transfer timeout, locking, recovery policy,
  and task cadence used by the target integration.

Suggested identity/build checks:

```text
git status --short
git branch --show-current
git rev-parse HEAD
git describe --tags --always --dirty
python scripts/generate_version.py check
python -m platformio run -e esp32s2dev
python -m platformio run -e esp32s3dev
```

Do not proceed as release-candidate evidence if the worktree or runtime
`version` output is dirty, or if the firmware identity does not match the
intended commit. A dirty run may be retained only as explicitly non-release
diagnostic evidence.

## Digital CLI Runs

The Arduino diagnostic CLI is an operator surface, not a production bus
manager. First validate the runner without hardware:

```text
python tools/run_i2c_hil.py --parser-test
python tools/run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted
```

Use a temporary output directory for each board/setup. Adjust address arguments
to the physical setup, list known-absent addresses explicitly, transfer any
required evidence to approved storage, then remove generated local output.

```text
python tools/run_i2c_hil.py --port <PORT> --address 0x48 --address 0x49 --absent-address 0x4A --absent-address 0x4B --suite targeted --stop-on-fail --out <EVIDENCE_DIR>
python tools/run_i2c_hil.py --port <PORT> --address 0x48 --address 0x49 --absent-address 0x4A --absent-address 0x4B --suite exhaustive --benchmark --stop-on-fail --out <EVIDENCE_DIR>
```

The contract verdict covers only CLI-observable behavior. An
`EVIDENCE_REQUIRED` verdict is expected where electrical observation is needed;
it is not a pass and must be closed with the analog, timing, comparator, or
fault evidence below. Preserve coarse transport mappings as observed. Do not
reinterpret a generic read failure as a proven address NACK.

## Electrical and Analog Procedure

- Keep every analog input within ADS1115 absolute limits; PGA selection does
  not raise those limits.
- Exercise all single-ended/differential MUX, PGA, and data-rate combinations
  required by the acceptance matrix. Record the applied DMM value, raw code,
  converted value, error, and predeclared tolerance.
- Capture conversion-ready behavior at 8, 128, and 860 SPS. Select instrument
  bandwidth/sample rate before the run; the 860 SPS pulse can be only several
  microseconds wide.
- Drive both traditional and window comparator crossings and verify polarity,
  latch clearing, and queue depths electrically.
- For address straps, record a separate physical setup for each address. Note
  the SDA-strap timing caveat for `0x4A`.

## Fault, Recovery, and Endurance Procedure

Use safe current limiting for stuck-line and brownout work. For each injected
fault, record the physical action, transaction stage when known, exact returned
`Status`, health/trust/dirty diagnostics, and the result of the explicit
recovery/resync path. Do not call an ambiguous partial write clean merely
because a later probe succeeds.

Choose positive soak duration, consecutive-failure limit, maximum command
latency, supply range, and thermal range before running. The runner rejects
duplicate or overlapping present/absent addresses and benchmark requests on a
suite that does not execute benchmarks. A typical automated command is:

```text
python tools/run_i2c_hil.py --port <PORT> --address <ADDR> --suite exhaustive --benchmark --soak --soak-duration-s <SECONDS> --soak-max-consecutive-failures <LIMIT> --soak-max-latency-s <SECONDS> --out <EVIDENCE_DIR>
```

Record start/end time, duration, cycles and commands, classified results,
maximum and mean latency, reset reason, final health/trust state, supply, and
ambient temperature. Serial disconnects and host exceptions are failures or
invalidated runs, not implicit passes.

The repeated soak workload is an endurance subset: blocking and continuous
reads, boundary gain/rate changes, scalar stress, a staged single-shot job,
health/probe, and recovery. The exhaustive prerequisite covers the other CLI
contracts once; neither phase replaces calibrated analog, ALERT/RDY,
comparator-crossing, physical-fault, shared-bus, supply, or thermal evidence.
For multiple populated addresses, one invocation shares the requested duration
across them; run a separate soak per address when the duration applies to each
device.

At every normal, timeout, failure-threshold, or handled-interrupt exit, the
runner performs a bounded epilogue: re-synchronize framing, cancel staged work,
recover every populated address, restore single-shot mode, gain 2 and rate 4,
then capture final settings and driver health. Any failed epilogue step fails
the soak. The summary distinguishes completed and partial cycles, lists
per-command counts and worst latency, and retains bounded individual failure
rows; commands skipped by `--stop-on-fail` are recorded as `NOT_RUN`.

## Native ESP-IDF Runs

On a host with the intended ESP-IDF toolchain, build and run the native example
on both targets:

```text
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s2 build
idf.py -C examples/esp_idf/basic -p <PORT> flash monitor
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic -p <PORT> flash monitor
```

Record the IDF version, board, wiring, bus-owner policy, exact error mapping,
and compact build/flash/monitor outcomes. Retain only small physical references
needed to support electrical claims. CI builds and Arduino runs do not
substitute for these physical native-IDF runs.

## Closeout

A release-facing result should contain only identity, a concise result matrix,
failures, remaining gaps, and stable physical-evidence references. Remove
resolved items from the active follow-up list. Never claim unrun hardware
coverage.
