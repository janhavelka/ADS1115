# ADS1115 Hardware Validation Results Template

Copy this file for each dated physical run. Keep only this concise record;
delete full serial/build transcripts and detailed runner output after the
result is extracted. Status values are `Pending`, `Pass`, `Fail`, `Blocked`, and
`Not applicable`. The `Pending` rows below are copy-time placeholders, not the
repository's live backlog; current unfinished work is in [`OPEN_ITEMS.md`](OPEN_ITEMS.md).

## Run Identity

| Field | Value |
| --- | --- |
| Start/end time and timezone | |
| Operator/reviewer | |
| Branch, full commit, tag/version | |
| Worktree and runtime identity clean? | |
| Firmware build timestamp | |
| Host OS and tool versions | |
| Exact build/flash/HIL commands | |
| Compact results file / physical-reference location | |

## Hardware and Integration Setup

| Field | Value |
| --- | --- |
| ESP32 board/revision | |
| ADS1115 module/vendor/revision | |
| VDD and ambient temperature | |
| I2C speed and SDA/SCL pull-ups | |
| ALERT/RDY pull-up | |
| ADDR strap / observed address | |
| Analog source and DMM | |
| Scope/logic analyzer and settings | |
| Wiring/photo reference | |
| Bus owner, locking, timeout, recovery policy | |
| Task cadence/workload | |

## Compact Evidence References

Do not attach or retain full serial/build transcripts. Link only the small
physical references necessary for claims that cannot be represented by the
result matrix.

| Evidence | Reference | Concise result |
| --- | --- | --- |
| Firmware/runtime identity | | |
| Analog instrument readings | | |
| I2C / ALERT-RDY capture | | |
| Wiring/photo/diagram | | |

## Results

List one row per tested setup/family. Split a family when results differ.

| Area | Required case | Status | Concise observed result | Evidence ID |
| --- | --- | --- | --- | --- |
| Identity | Clean commit/version/build match | Pending | | |
| Arduino HIL | ESP32-S2 targeted/full plan | Pending | | |
| Arduino HIL | ESP32-S3 targeted/full plan | Pending | | |
| Address | Physical `0x48`, `0x49`, `0x4A`, `0x4B` straps | Pending | | |
| Address | Expected-absent and unplugged device | Pending | | |
| Analog | 4 single-ended and 4 differential MUX choices | Pending | | |
| Analog | 6 PGA ranges with measured safe inputs | Pending | | |
| Timing | 8 data rates and single-shot readiness | Pending | | |
| ALERT/RDY | 8, 128, and 860 SPS electrical captures | Pending | | |
| Comparator | Traditional/window, polarity, latch, queue | Pending | | |
| Fault | Stuck SDA/SCL, unplug/replug, brownout/reset | Pending | | |
| Trust/recovery | Raw/partial/ambiguous write, dirty state, resync | Pending | | |
| Shared bus | Contention, deadlines, cancellation, task cadence | Pending | | |
| Native ESP-IDF | ESP32-S2 physical run | Pending | | |
| Native ESP-IDF | ESP32-S3 physical run | Pending | | |
| Endurance | Predeclared nominal soak and worst-rate stress | Pending | | |
| Final board | Electrical, analog, disconnect, calibration acceptance | Pending | | |

## Quantitative Summary

| Metric | Acceptance limit | Observed | Result |
| --- | ---: | ---: | --- |
| Analog error by MUX/PGA family | | | |
| Sample cadence by data rate | | | |
| Maximum command/callback latency | | | |
| Soak duration / cycles / commands | | | |
| Failures / resets / recoveries | | | |

## Unfinished Issues Only

Move a resolved failure into the dated result/evidence row above; do not retain
closed work in this active list.

| Issue or blocker | Reproduction/impact | Evidence | Owner | Next action |
| --- | --- | --- | --- | --- |
| | | | | |

## Decision

| Field | Value |
| --- | --- |
| Release/final-board decision | |
| Unvalidated claims explicitly excluded | |
| Reviewer and date | |
