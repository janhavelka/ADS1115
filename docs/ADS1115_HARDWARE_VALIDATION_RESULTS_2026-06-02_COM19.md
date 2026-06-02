# ADS1115 Hardware Validation Results - 2026-06-02 COM19

This report records the corrected automated HIL/CLI capture run after the
address-restore sequencing fix. It is limited hardware evidence for the attached
COM19 setup. It is not full release hardware validation.

## Run Identity

| Field | Value |
| --- | --- |
| Date/time started | 2026-06-02 20:52:01 |
| Date/time ended | 2026-06-02 20:52:40 |
| Branch | `hardening/ads1115-industry-standard-p0` |
| Host helper commit | `e32822341e251a821febe4710a6879f1aff08312` |
| Firmware/library commit reported by CLI | `9551bee` clean |
| Library version reported by CLI | `1.0.0` |
| Firmware build timestamp | 2026-06-02 20:47:43 |
| Serial port | `COM19` |
| Raw transcript | `hil_logs/ads1115_hil_20260602_205201.log` |

The raw transcript is stored under ignored `hil_logs/` in the working tree.
Archive it with release evidence if this run is used for a release gate.

## Hardware Setup

| Field | Value |
| --- | --- |
| ESP32 board | ESP32-S2 class board; upload earlier identified `ESP32-S2FH4` |
| ADS1115 addresses present | `0x48`, `0x49` |
| ADS1115 addresses absent in this setup | `0x4A`, `0x4B` |
| ADS1115 module/vendor/revision | Not recorded |
| ADS1115 VDD | Not recorded |
| I2C speed | Not recorded in transcript |
| SDA/SCL pull-ups | Not recorded |
| Instruments/photos | Not recorded |

## Command Sequence

The run used `tools/hil_ads1115_capture.py` with the corrected sequence:

```text
version
addr
state
cfg
drv
addr 0x48
probe
cfg
selftest
stress 500
stress_mix 200
addr 0x49
probe
cfg
selftest
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
stress 1000
stress_mix 200
drv
```

## Results

| Area | Case | Result | Evidence |
| --- | --- | --- | --- |
| Identity | Firmware version/build/commit printed | Pass | CLI reported version `1.0.0`, firmware commit `9551bee` clean |
| Initial state | `cfg` before functional tests | Pass | `Initialized: YES`, `State: READY`, address `0x48`, dirty `NO` |
| Address | `0x48` present | Pass | `Address check 0x48: present/pass (initialized OK)` |
| Address | `0x49` present | Pass | `Address check 0x49: present/pass (initialized OK)` |
| Address | `0x4A` absent negative check | Pass | `Address check 0x4A: absent/pass-as-negative-test (I2C_ERROR)` |
| Address | `0x4B` absent negative check | Pass | `Address check 0x4B: absent/pass-as-negative-test (I2C_ERROR)` |
| CLI state clarity | Failed `addr 0x4A` preserved initialized driver | Pass | Active/initialized remained `0x49`; requested shown as `0x4A`; last error shown |
| CLI state clarity | Failed `addr 0x4B` preserved initialized driver | Pass | Active/initialized remained `0x48`; requested shown as `0x4B`; last error shown |
| Restore | Restore to `0x48` after `0x4A` negative check | Pass | `addr 0x48`, `probe`, `cfg`, `selftest` all succeeded |
| Restore | Restore to `0x48` after `0x4B` negative check | Pass | `addr 0x48`, `probe`, `cfg`, `selftest` all succeeded |
| Selftest | `0x48` safe command selftest | Pass | `pass=29 fail=0 skip=0` |
| Selftest | `0x49` safe command selftest | Pass | `pass=29 fail=0 skip=0` |
| Stress | `stress 500` on `0x48` | Pass | `500` success, `0` errors, `100.00%` |
| Stress | First `stress_mix 200` on `0x48` | Pass | `ok=200 fail=0`, `100.00%` |
| Stress | Final `stress 1000` after address restores | Pass | `1000` success, `0` errors, `100.00%` |
| Stress | Final `stress_mix 200` after address restores | Pass | `ok=200 fail=0`, `100.00%` |
| Final state | Driver health | Pass | `READY`, online `yes`, consecutive failures `0`, total failures `0`, last error `never` |

## Not Covered By This Run

- ALERT/RDY pulse captures at 8, 128, and 860 SPS.
- Comparator electrical behavior with external thresholds.
- Full mux, gain, and data-rate sweeps with measured input sources.
- Stuck bus, unplug/replug, brownout/reset, and partial-write fault injection.
- 24-hour nominal soak and 2-hour 860 SPS stress.
- Pure ESP-IDF hardware run.
- Complete operator metadata, wiring photos, pull-up values, VDD, and
  instrument records.

## Conclusion

The corrected automated HIL sequence is valid evidence for address selection
state clarity, absent-address negative checks, restore-before-functional
sequencing, safe selftests on initialized addresses, and short stress runs on
the connected COM19 setup. It does not complete the full hardware validation
plan or justify field-grade release claims.
