# ADS1115 Hardware Validation Results Template

Do not prefill pass/fail results. Copy this template for each dated hardware
validation run and attach the raw evidence files listed below.

## Run Identity

| Field | Value |
| --- | --- |
| Date/time started | |
| Date/time ended | |
| Operator | |
| Branch | |
| Commit | |
| Library version | |
| Firmware build timestamp | |
| Host OS | |
| PlatformIO version | |
| ESP-IDF version, if used | |
| Evidence directory or archive | |

## Hardware Setup

| Field | Value |
| --- | --- |
| ESP32 board | |
| ADS1115 module/vendor/revision | |
| ADS1115 VDD | |
| I2C speed | |
| SDA/SCL pull-ups | |
| ALERT/RDY pull-up | |
| ADDR strap | |
| Analog source | |
| DMM/instrument model | |
| Scope/logic analyzer model | |
| Ambient temperature | |
| Wiring notes/photos | |

## Build And Firmware Evidence

| Artifact | Path/link | Notes |
| --- | --- | --- |
| `git branch --show-current` output | | |
| `git rev-parse HEAD` output | | |
| `python scripts/generate_version.py check` output | | |
| Arduino ESP32-S2 build log | | |
| Arduino ESP32-S3 build log | | |
| ESP-IDF ESP32-S2 build log | | |
| ESP-IDF ESP32-S3 build log | | |
| Firmware serial startup log | | |

## Evidence Files

| Evidence | Path/link | Timestamp | Notes |
| --- | --- | --- | --- |
| Arduino CLI serial log | | | |
| ESP-IDF monitor log | | | |
| I2C logic capture | | | |
| ALERT/RDY pulse capture at 8 SPS | | | |
| ALERT/RDY pulse capture at 128 SPS | | | |
| ALERT/RDY pulse capture at 860 SPS | | | |
| Wiring photos | | | |
| Soak/stress logs | | | |

## Result Matrix

Status values: `Pending`, `Pass`, `Fail`, `Blocked`, `Not applicable`.

| Area | Case | Status | Evidence | Notes |
| --- | --- | --- | --- | --- |
| Identity | Branch/commit/version/build recorded | Pending | | |
| Address | ADDR to GND, `0x48` | Pending | | |
| Address | ADDR to VDD, `0x49` | Pending | | |
| Address | ADDR to SDA, `0x4A` | Pending | | |
| Address | ADDR to SCL, `0x4B` | Pending | | |
| Address | Wrong selected address | Pending | | |
| Address | Missing/unplugged ADS1115 | Pending | | |
| MUX | AIN0-GND raw/voltage | Pending | | |
| MUX | AIN1-GND raw/voltage | Pending | | |
| MUX | AIN2-GND raw/voltage | Pending | | |
| MUX | AIN3-GND raw/voltage | Pending | | |
| MUX | AIN0-AIN1 differential | Pending | | |
| MUX | AIN0-AIN3 differential | Pending | | |
| MUX | AIN1-AIN3 differential | Pending | | |
| MUX | AIN2-AIN3 differential | Pending | | |
| Gain | +/-6.144 V range | Pending | | |
| Gain | +/-4.096 V range | Pending | | |
| Gain | +/-2.048 V range | Pending | | |
| Gain | +/-1.024 V range | Pending | | |
| Gain | +/-0.512 V range | Pending | | |
| Gain | +/-0.256 V range | Pending | | |
| Data rate | 8 SPS | Pending | | |
| Data rate | 16 SPS | Pending | | |
| Data rate | 32 SPS | Pending | | |
| Data rate | 64 SPS | Pending | | |
| Data rate | 128 SPS | Pending | | |
| Data rate | 250 SPS | Pending | | |
| Data rate | 475 SPS | Pending | | |
| Data rate | 860 SPS | Pending | | |
| Mode | Single-shot conversion | Pending | | |
| Mode | Continuous conversion | Pending | | |
| Timing | Blocking read path | Pending | | |
| Timing | `service()` / `tick()` path | Pending | | |
| Comparator | Traditional comparator | Pending | | |
| Comparator | Window comparator | Pending | | |
| Comparator | Latch behavior | Pending | | |
| Comparator | Polarity behavior | Pending | | |
| Comparator | Queue behavior | Pending | | |
| ALERT/RDY | Conversion-ready capture at 8 SPS | Pending | | |
| ALERT/RDY | Conversion-ready capture at 128 SPS | Pending | | |
| ALERT/RDY | Conversion-ready capture at 860 SPS | Pending | | |
| Fault | Stuck bus | Pending | | |
| Fault | Unplug/replug | Pending | | |
| Fault | Brownout/reset | Pending | | |
| Fault | Manual `recover()` | Pending | | |
| Fault | Raw write dirty-state visibility | Pending | | |
| Fault | Partial write/fault injection, if available | Pending | | |
| Platform | Arduino ESP32-S2 CLI | Pending | | |
| Platform | Arduino ESP32-S3 CLI | Pending | | |
| Platform | ESP-IDF ESP32-S2 example | Pending | | |
| Platform | ESP-IDF ESP32-S3 example | Pending | | |
| Stress | 24-hour nominal soak | Pending | | |
| Stress | 2-hour 860 SPS stress | Pending | | |

## Deviations

| Deviation | Impact | Disposition |
| --- | --- | --- |
| | | |

## Failures And Follow-Up

| Failure | Reproduction | Evidence | Owner | Status |
| --- | --- | --- | --- | --- |
| | | | | |

## Sign-Off

| Role | Name | Date | Notes |
| --- | --- | --- | --- |
| Operator | | | |
| Reviewer | | | |
| Release decision | | | |
