# ADS1115 Documentation

Only current contracts, open validation work, and preserved technical source
material belong here.

## Current documents

| Document | Purpose |
| --- | --- |
| [`../README.md`](../README.md) | User guide, owner-safe API contract, examples, and reproducible checks. |
| [`OPEN_ITEMS.md`](OPEN_ITEMS.md) | Outstanding hardware and integration evidence only. |
| [`IDF_PORT.md`](IDF_PORT.md) | ESP-IDF adapter ownership, timeout, and error-mapping contract. |
| [`ADS1115_HARDWARE_VALIDATION_PLAN.md`](ADS1115_HARDWARE_VALIDATION_PLAN.md) | Procedure for current physical validation. |
| [`ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md`](ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md) | Dated hardware-result template. |
| [`../CHANGELOG.md`](../CHANGELOG.md) | Release history and migration notes. |

Historical HIL transcripts, generated summaries, completed audit reports, and
prompt-era hardening documents are not retained in the active tree. Required
lab evidence belongs in the approved evidence store and is referenced from a
dated result.

## API reference

Run `doxygen Doxyfile` from the repository root. Undocumented public members,
missing parameter/return contracts, malformed comments, and unresolved
references fail the build. Generated HTML is written to ignored
`docs/doxygen/html/`; the authoritative source remains in the public headers
and current Markdown files.

## Protected reference material

[`reference/`](reference/) is deliberately retained. It contains the TI
ADS111x Rev. E datasheet, curated datasheet-derived Markdown transcripts in
`extracted-md/`, and an upstream TI reference-driver snapshot. These files are
technical source material, not validation evidence or library implementation.
