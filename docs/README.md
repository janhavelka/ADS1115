# ADS1115 Documentation Index

This directory separates release-facing documentation from historical hardening
material and raw evidence.

## Release-Facing Documents

| Document | Purpose |
| --- | --- |
| `../README.md` | Primary user documentation, API behavior, examples, and validation summary. |
| `../CHANGELOG.md` | Release history, release notes, and migration notes. |
| `IDF_PORT.md` | ESP-IDF integration contract, adapter requirements, and error-mapping limits. |
| `ADS1115_HARDWARE_VALIDATION_PLAN.md` | Operator procedure for complete hardware validation. |
| `ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md` | Blank template for dated hardware validation runs. |
| `ADS1115_HARDWARE_VALIDATION_RESULTS_2026-06-02_COM19.md` | Limited COM19 HIL evidence for address handling, restore sequencing, selftests, and short stress. |
| `ADS1115_RELEASE_VALIDATION_SUMMARY_2026-06-25.md` | Compact release validation summary for the recent COM8 targeted HIL and 20-hour soak, including remaining evidence gates. |
| `TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md` | Exact v1.2.0 integration audit plus finding-by-finding 2.0 implementation disposition and remaining product gates. |

## Evidence

Tracked release evidence is stored under `evidence/`.

| Path | Contents |
| --- | --- |
| `evidence/hil/2026-06-02_COM19/ads1115_hil_20260602_205201.log` | Raw transcript for the limited COM19 HIL run. |

Local capture logs under `hil_logs/` remain ignored unless explicitly copied
into `docs/evidence/`.

## Datasheet And Reference Material

| Path | Contents |
| --- | --- |
| `reference/ADS111x_datasheet_revE.pdf` | TI ADS111x datasheet used for contract checks. |
| `reference/extracted-md/` | Compact, curated datasheet notes. |
| `reference/pdf-extracted-md/` | Full PDF text extraction for search and traceability. |
| `reference/TI_registry_reference/` | TI reference driver snapshot and extraction notes. |

## Historical Material

Historical prompts, audit reports, and hardening reports are kept under
`archive/`. They are useful for provenance, but they are not the current release
readiness statement.
