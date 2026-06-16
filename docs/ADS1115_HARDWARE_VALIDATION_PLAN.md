# ADS1115 Hardware Validation Plan

This plan prepares hardware-in-the-loop validation for the ADS1115 hardening
branch. It is an operator procedure, not validation evidence. Do not mark a
case passed without dated logs, captures, and hardware identity recorded in the
results template.

## Evidence Rules

- Record branch, commit, library version, build timestamp, board, ADS1115 module,
  VDD, pull-up values, bus speed, ambient conditions, wiring, instruments, and
  operator.
- Save raw serial logs and scope/logic-analyzer captures with timestamps.
- Record exact commands used and exact observed output.
- Mark unrun cases as `Pending`; mark blocked cases with the blocker.
- Do not infer address-NACK/data-NACK precision from adapters that only expose
  coarse transport errors.

## Required Equipment

| Item | Requirement |
| --- | --- |
| Host | PlatformIO and serial terminal; ESP-IDF `idf.py` when running native IDF hardware checks |
| Boards | ESP32-S2 and ESP32-S3 targets used by the examples |
| ADC | ADS1115 module or reference board with accessible ADDR, ALERT/RDY, SDA, SCL, VDD, GND, AIN0..AIN3 |
| Instruments | DMM for DC levels; oscilloscope or logic analyzer for I2C and ALERT/RDY pulse capture |
| Pull-ups | Document SDA/SCL and ALERT/RDY pull-up values and voltage domains |
| Supplies | Current-limited supply capable of brownout/reset tests |

## Test Identity

Record these fields before starting:

| Field | Operator entry |
| --- | --- |
| Date/time | |
| Operator | |
| Branch | |
| Commit | |
| Library version | |
| Firmware build timestamp | |
| Host OS/tool versions | |
| ESP32 board | |
| ADS1115 module/vendor/revision | |
| ADS1115 VDD | |
| SDA/SCL pull-ups | |
| ALERT/RDY pull-up | |
| I2C speed | |
| Ambient temperature | |
| Wiring notes | |
| Instruments and settings | |
| Evidence directory | |

Suggested host commands:

```bash
git branch --show-current
git rev-parse HEAD
python scripts/generate_version.py check
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
```

Suggested Arduino CLI identity commands:

```text
version
addr
state
cfg
drv
```

## Arduino CLI Capture

Use the diagnostic Arduino CLI for most operator-driven validation. The CLI is
not a production shared-bus manager; it is the HIL command surface.

Build and upload with the appropriate environment and port:

```bash
python -m platformio run -e esp32s3dev --target upload --upload-port <PORT>
python -m platformio run -e esp32s2dev --target upload --upload-port <PORT>
```

Capture logs manually or with:

```bash
python tools/hil_ads1115_capture.py --dry-run --suite identity
python tools/hil_ads1115_capture.py --port <PORT> --suite identity --suite address --out-dir hil_logs
python tools/hil_ads1115_capture.py --port <PORT> --suite all --out-dir hil_logs
```

The automated helper treats missing address checks as negative tests and
restores `0x48` before functional groups. Functional evidence is valid only
when the preceding `cfg` shows `Initialized: YES` and `State: READY`.

## Address Strap Tests

Run each ADDR strap as a separate physical setup. Photograph or record the strap
for the evidence bundle.

| Strap | Expected address | Commands | Required evidence |
| --- | --- | --- | --- |
| ADDR to GND | `0x48` | `addr 0x48`, `probe`, `cfg`, `read` | Serial log, I2C capture optional |
| ADDR to VDD | `0x49` | `addr 0x49`, `probe`, `cfg`, `read` | Serial log |
| ADDR to SDA | `0x4A` | `addr 0x4A`, `probe`, `cfg`, `read` | Serial log plus note that SDA strap timing caveat was considered |
| ADDR to SCL | `0x4B` | `addr 0x4B`, `probe`, `cfg`, `read` | Serial log |

Wrong/missing address tests:

```text
addr 0x48
probe
addr 0x49
probe
addr 0x4A
probe
cfg
addr 0x48
probe
cfg
selftest
addr 0x4B
probe
cfg
addr 0x48
probe
cfg
selftest
```

Expected evidence: the selected strapped address probes successfully; the other
addresses fail with the adapter's observed transport/status mapping and do not
corrupt the initialized driver address or transport callbacks. Do not claim
precise address-NACK unless the adapter and bus capture prove it.

## MUX Raw And Voltage Tests

Use safe input levels inside ADS1115 absolute input limits (`GND - 0.3 V` to
`VDD + 0.3 V`). For each case, record applied voltage, DMM measurement, raw
code, calculated voltage, and tolerance.

```text
gain 2
rate 4
mode single
ch 0
read
readv
ch 1
read
readv
ch 2
read
readv
ch 3
read
readv
diff 0
read
readv
diff 1
read
readv
diff 2
read
readv
diff 3
read
readv
```

## PGA/Gain Tests

Use safe source levels for each full-scale range. PGA selection does not raise
absolute input limits.

```text
gain 0
readv
gain 1
readv
gain 2
readv
gain 3
readv
gain 4
readv
gain 5
readv
timing
```

Required evidence: DMM voltage, selected gain, raw value, reported voltage, and
tolerance for each gain.

## Data-Rate Tests

For each rate, record the configured rate, nominal conversion time, and observed
sample cadence from serial timestamps or a logic analyzer.

```text
rate 0
timing
stress 20
rate 1
timing
stress 20
rate 2
timing
stress 20
rate 3
timing
stress 20
rate 4
timing
stress 50
rate 5
timing
stress 50
rate 6
timing
stress 100
rate 7
timing
stress 100
```

## Conversion Mode And Timing Paths

Single-shot, blocking, and readiness:

```text
mode single
start
poll
read
readv
stress 50
```

Continuous mode:

```text
mode cont
poll
raw
voltage
stress 50
mode single
```

Service/tick path evidence: the Arduino CLI calls `service()` in the main loop
and readiness waits call `tick()`/readiness paths during stress/selftest. Record
`state`, `drv`, and `selftest` output after the runs.

## Comparator And ALERT/RDY

Traditional comparator:

```text
comp
comp mode trad
comp pol low
comp latch 0
comp queue 1
comp th <low_code> <high_code>
comp
```

Window comparator:

```text
comp mode window
comp pol high
comp latch 1
comp queue 2
comp th <low_code> <high_code>
comp
```

Required evidence: stimulus voltage, threshold codes, ALERT/RDY level, polarity,
latch behavior, queue behavior, and a scope/logic-analyzer capture when the pin
is used.

ALERT/RDY conversion-ready pulse capture:

```text
comp rdy
rate 0
start
poll
rate 4
start
poll
rate 7
start
poll
comp disable
```

Capture ALERT/RDY at 8, 128, and 860 SPS. At 860 SPS, the continuous-mode pulse
can be approximately 8 us; use an instrument sample rate that can prove whether
the pulse was present and whether the host strategy could capture it.

## Fault Injection And Recovery

Wrong address and missing target:

```text
addr <wrong_address>
probe
read
drv
recover
drv
```

Unplug/replug:

```text
mode single
read
# unplug ADS1115
read
drv
# replug ADS1115
recover
read
drv
```

Stuck bus: hold SDA or SCL low only with safe hardware current limiting.

```text
read
drv
recover
drv
```

Brownout/reset: lower ADS1115 VDD until failure is observed, then restore.

```text
read
drv
recover
read
```

Raw diagnostic partial-state visibility:

```text
wreg 1 0x8583
cfg
drv
recover
cfg
```

Required evidence: exact status code/detail/message, dirty-state visibility,
recovery behavior, and physical fault description. Partial write/fault injection
with an I2C fault injector is optional but should record where the transaction
was interrupted.

## ESP-IDF Example Hardware Checks

Local build commands when `idf.py` is installed:

```bash
idf.py --version
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic -p <PORT> flash monitor
idf.py -C examples/esp_idf/basic set-target esp32s2 build
idf.py -C examples/esp_idf/basic -p <PORT> flash monitor
```

Required evidence: build logs, monitor logs, board identity, ADS1115 wiring, and
whether errors are coarse ESP-IDF mappings or instrument-proven bus faults.

## Soak And Stress

Nominal 24-hour soak:

```text
version
addr
cfg
stress 100000
drv
```

Two-hour 860 SPS stress:

```text
rate 7
mode single
stress 100000
stress_mix 10000
drv
```

Record start/end timestamps, supply voltage, ambient temperature, reset reason,
failure count, health state, and any transport errors.

## Required Artifacts

- Completed `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`.
- Raw serial logs for every board/run.
- Build logs for Arduino and ESP-IDF examples.
- Scope/logic-analyzer captures for I2C and ALERT/RDY pulse tests.
- Photos or wiring diagrams for each ADDR strap setup.
- Operator notes for deviations, blocked tests, and environmental conditions.
