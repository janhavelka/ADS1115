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
| [`evidence/hil/`](evidence/hil/) | Index of earlier fixture evidence, limitations, and retained artifact hashes. |

Long serial transcripts are not review documents. Keep concise outcomes,
failures, fixture identity, commands, and hashes; retain a raw artifact only
when it adds reproducible evidence that the summary cannot preserve.

## Reference Material

[`reference/`](reference/) contains the TI ADS111x Rev. E datasheet, compact
derived notes, and an upstream TI reference-driver snapshot. It supports
contract review but is not release evidence.

## Archive

[`archive/`](archive/) contains completed release records, prompts, audits,
merge reports, and hardening journals. Any checklist or "remaining" section
there describes its dated revision and is not the active backlog. The live
backlog is [`OPEN_ITEMS.md`](OPEN_ITEMS.md).
