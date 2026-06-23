# ADS1115 COM8 Targeted HIL Report - 2026-06-23

## Scope

- Board: ESP32-S2 on COM8
- Hardware: two ADS1115 devices detected at `0x48` and `0x49`
- Firmware: Arduino diagnostic CLI from this working tree
- Runner: `tools/run_i2c_hil.py`
- Suite: `targeted`

The flashed firmware reported:

```text
ADS1115 library full: 1.1.0 (8476da2, 2026-06-23 11:35:29, dirty)
```

Because the firmware was built from a dirty working tree, this is functional
validation evidence for the current patch set, not clean release-candidate
evidence.

## Command

```powershell
python tools\run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite targeted --timeout-s 8 --idle-s 0.5 --boot-settle-s 2 --stop-on-fail
```

Before the passing run, `pio run -e esp32s2dev -t upload --upload-port COM8`
left the chip in the flasher stub as expected from `board_upload.after_reset =
no_reset_stub`. The stub was exited with PlatformIO's bundled esptool 4.8.9:

```powershell
python "$env:USERPROFILE\.platformio\packages\tool-esptoolpy@src-e9520c52db7d0ecbb98379d0d58b38a9\esptool.py" --chip esp32s2 --port COM8 run
```

## Results

- Transcript: `hil_logs\ads1115_hil_20260623_113917.log`
- Summary: `hil_logs\ads1115_hil_20260623_113917_summary.md`
- PASS: 154
- FAIL: 0
- UNKNOWN: 24
- NOT_RUN: 0
- Contract verdict: PASS
- Evidence verdict: UNKNOWN
- Final verdict: UNKNOWN

The `UNKNOWN` rows are analog/electrical evidence rows where serial tokens
matched but no calibrated source, DMM reading, or ALERT/RDY capture was attached.
They are not contract failures. They still block any production-grade release
claim until the hardware validation plan is completed with external evidence.

## Covered Contract Areas

- Two-address selection with `addr 0x48` and `addr 0x49`
- Probe, settings, health, and recovery commands
- Data-rate boundary settings and timing reports
- PGA boundary settings
- Single-ended and differential mux command coverage
- Single-shot and continuous-mode command paths
- Comparator mode, polarity, latch, queue, threshold, and conversion-ready setup
- Register reads, full config write/readback, raw threshold write dirty state,
  and recovery dirty-state clearing
- Staged single-shot and apply jobs with budgets `0`, `1`, and `255`
- Active-job interleaving guard returning `BUSY`
- Short scalar and mixed stress commands
- Malformed input rejection without transport-health failures

## Remaining Production Evidence Gaps

- Clean release-candidate HIL from `git diff --exit-code` firmware.
- Full HIL and soak from the clean release commit.
- Analog accuracy fixture rows with applied source voltage, DMM voltage, raw
  code, driver voltage, and pass/fail thresholds.
- ALERT/RDY and comparator electrical captures.
- Physical fault matrix: missing device, unplug/replug, stuck SDA/SCL,
  brownout/reset, and recovery.
- Pure ESP-IDF hardware validation on ESP32-S2 and ESP32-S3.
