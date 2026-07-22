# ADS1115 Documentation

Use this page to find current contracts, unfinished work, compact dated
evidence, and preserved technical reference material.

## Current

| Document | Purpose |
| --- | --- |
| [`../README.md`](../README.md) | User guide, owner-safe API contract, examples, and reproducible checks. |
| [`OPEN_ITEMS.md`](OPEN_ITEMS.md) | Single index of unfinished release evidence, CI maintenance, and product integration work. |
| [`IDF_PORT.md`](IDF_PORT.md) | ESP-IDF adapter ownership, timeout, and error-mapping contract. |
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

[`reference/`](reference/) is deliberately retained for AI-assisted and human
contract review. It contains the TI ADS111x Rev. E datasheet, the curated
datasheet-derived Markdown transcripts in `extracted-md/`, and an upstream TI
reference-driver snapshot. It is technical source material, not release
evidence.
