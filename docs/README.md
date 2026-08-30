# ADS1115 Documentation

Only current contracts, open work, and the datasheet reference corpus belong
here.

## Current documents

| Document | Purpose |
| --- | --- |
| [`../README.md`](../README.md) | User guide, owner-safe API contract, examples, and reproducible checks. |
| [`CODE_AUDIT_REPORT.md`](CODE_AUDIT_REPORT.md) | Disposition and implementation report for the completed 2026-08-30 code audit. |
| [`OPEN_ITEMS.md`](OPEN_ITEMS.md) | Outstanding hardware and integration evidence only. |
| [`IDF_PORT.md`](IDF_PORT.md) | ESP-IDF adapter ownership, timeout, and error-mapping contract. |
| [`ADS1115_HARDWARE_VALIDATION_PLAN.md`](ADS1115_HARDWARE_VALIDATION_PLAN.md) | Procedure for physical validation, including the evidence-retention rule. |
| [`ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`](ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md) | Dated hardware-result template. |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release history and migration notes. |

## API reference

Run `doxygen Doxyfile` from the repository root. Undocumented public members,
missing parameter/return contracts, malformed comments, and unresolved
references fail the build. Generated HTML is written to ignored
`docs/doxygen/html/`; the authoritative source remains in the public headers
and current Markdown files.

## Protected reference material

[`reference/`](reference/) contains the TI ADS111x Rev. E datasheet, its full
PDF-derived Markdown transcript, and curated topic transcripts in
`extracted-md/`. The PDF is authoritative for every hardware behavior claim in
this repository.
