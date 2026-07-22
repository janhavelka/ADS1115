# HIL Evidence Index

Only compact evidence summaries are kept in the repository. Long serial and
instrument transcripts belong in an immutable external archive; summaries must
record their hashes, sizes, dates, hardware identity, commands, outcomes, and
limitations.

| Evidence | Firmware | What it establishes | Current use |
| --- | --- | --- | --- |
| [`2026-06-02_COM19/README.md`](2026-06-02_COM19/README.md) | 1.0.0, clean short commit `9551bee` | Limited address selection/restore, safe selftest, and short stress on one ESP32-S2-class COM19 setup | Historical only; predates 2.0 |
| [`2026-06-25_COM8/README.md`](2026-06-25_COM8/README.md) | 1.1.0, dirty short commit `8476da2` | Broad CLI runs and 8-hour/20-hour digital soaks on one ESP32-S2 COM8 setup | Historical diagnostic evidence only; dirty firmware and predates 2.0 |

## Current Gap

There is no repository record of a physical 2.0 HIL run. Parser tests and
dry-runs validate the host runner only. Current electrical, analog, fault,
native ESP-IDF, ESP32-S2/ESP32-S3, and final-board acceptance gates remain in
`docs/ADS1115_HARDWARE_VALIDATION_PLAN.md`.
