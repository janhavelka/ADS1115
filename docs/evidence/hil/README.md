# HIL Evidence Index

Only compact evidence summaries are kept in the repository. Long serial and
instrument transcripts belong in an immutable external archive; summaries must
record their hashes, sizes, dates, hardware identity, commands, outcomes, and
limitations.

| Evidence | Firmware | What it establishes | Current use |
| --- | --- | --- | --- |
| [`2026-06-02_COM19/README.md`](2026-06-02_COM19/README.md) | 1.0.0, clean short commit `9551bee` | Limited address selection/restore, safe selftest, and short stress on one ESP32-S2-class COM19 setup | Historical only; predates 2.0 |
| [`2026-06-25_COM8/README.md`](2026-06-25_COM8/README.md) | 1.1.0, dirty short commit `8476da2` | Broad CLI runs and 8-hour/20-hour digital soaks on one ESP32-S2 COM8 setup | Historical diagnostic evidence only; dirty firmware and predates 2.0 |
| [`2026-07-22_COM6/README.md`](2026-07-22_COM6/README.md) | 2.0.0, clean commit `dd25d61` | Two-device exhaustive ESP32-S2 coverage, absent-address checks, 87,956-command one-hour soak, and post-soak smoke with zero digital failures | Current diagnostic baseline; physical and product evidence remains open |

## Current Gap

A clean ESP32-S2 Arduino diagnostic HIL baseline now exists for v2.0. Current
electrical, calibrated analog, injected-fault, native ESP-IDF, ESP32-S3,
shared-bus product workload, and final-board acceptance gates remain in
`docs/ADS1115_HARDWARE_VALIDATION_PLAN.md`.
