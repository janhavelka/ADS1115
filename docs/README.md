# ADS1115 Documentation

Use this page to distinguish current contracts and unfinished work from dated
evidence and historical implementation reports.

## Current

| Document | Purpose |
| --- | --- |
| [`../README.md`](../README.md) | User guide, owner-safe API contract, examples, and reproducible checks. |
| [`OPEN_ITEMS.md`](OPEN_ITEMS.md) | Single index of unfinished release evidence, CI maintenance, and product integration work. |
| [`IDF_PORT.md`](IDF_PORT.md) | ESP-IDF adapter ownership, timeout, and error-mapping contract. |
| [`TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md`](TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md) | Unfinished TunnelMonitor product, adapter, capacity, and final-board gates only. |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release history and migration notes. |

## Hardware Validation

| Document | Purpose |
| --- | --- |
| [`ADS1115_HARDWARE_VALIDATION_PLAN.md`](ADS1115_HARDWARE_VALIDATION_PLAN.md) | Procedure for the still-open v2.0 physical evidence gates. |
| [`ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`](ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md) | Compact dated-results template. |
| [`evidence/hil/`](evidence/hil/) | Index of compact dated fixture outcomes and limitations, including the current v2 ESP32-S2 baseline. |

Do not retain full serial transcripts or detailed generated runner summaries.
After verifying the dated fixture identity, concise outcomes, failures, and
limitations, delete the raw HIL output.

## API Reference

Run `doxygen Doxyfile` from the repository root. Undocumented public members,
missing parameter/return contracts, malformed comments, and unresolved
references fail the build. Generated HTML is written to ignored
`docs/doxygen/html/`; source documentation remains in the public headers and
the current Markdown pages listed above.

## Reference Material

[`reference/`](reference/) contains the TI ADS111x Rev. E datasheet, compact
derived notes, and an upstream TI reference-driver snapshot. It supports
contract review but is not release evidence.

## Archive

[`archive/`](archive/) contains completed release records, prompts, audits,
merge reports, and hardening journals. Any checklist or "remaining" section
there describes its dated revision and is not the active backlog. The live
backlog is [`OPEN_ITEMS.md`](OPEN_ITEMS.md).
