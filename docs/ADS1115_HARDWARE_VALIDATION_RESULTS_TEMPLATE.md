# ADS1115 Hardware Validation Results Template

Copy this file for each dated physical run. Keep this repository record
concise; store lengthy raw artifacts externally and identify them in the
manifest. Status values are `Pending`, `Pass`, `Fail`, `Blocked`, and
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
| Evidence archive location | |

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

## Evidence Manifest

Every external artifact needs a stable location and integrity metadata. Do not
paste full serial or build transcripts into this report.

| Artifact | Location | Captured at | Bytes | SHA-256 | Retention |
| --- | --- | --- | ---: | --- | --- |
| Firmware/build record | | | | | |
| CLI transcript and generated summary | | | | | |
| ESP-IDF build/monitor record | | | | | |
| Analog measurements | | | | | |
| I2C / ALERT-RDY captures | | | | | |
| Wiring photos/diagram | | | | | |
| Fault and soak records | | | | | |

## Results

List one row per tested setup/family and put detailed values in the referenced
artifact. Split a family into separate rows when results differ.

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
