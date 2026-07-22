# ADS1115 compact documentation inventory

This directory summarizes ADS1115 facts from the checked-in ADS111x Rev. E
datasheet: ADDR-selected I2C addresses, address-pointer transactions, four
16-bit registers, reset values, PGA scaling, data-rate settings,
comparator/ready behavior, and ADS1113/ADS1114/ADS1115 differences.

| File | Purpose |
| --- | --- |
| `00_document_inventory.md` | Map of compact notes and source material. |
| `01_chip_overview.md` | ADS111x family overview with ADS1115-specific emphasis. |
| `02_pinout_and_signals.md` | Pins, analog inputs, address pin, ALERT/RDY, and bus signals. |
| `03_electrical_and_timing.md` | Supply/input limits, PGA scales, data rates, and I2C timing. |
| `04_protocol_commands_and_transactions.md` | Address pointer, register reads/writes, general call reset, and high-speed mode. |
| `05_register_map.md` | Four-register map and key bit fields. |
| `06_modes_interrupts_status_and_faults.md` | Single-shot/continuous modes, comparator, conversion-ready pin, and status behavior. |
| `07_initialization_reset_and_operational_notes.md` | Startup, reset, sampling sequence, scaling, and operational cautions. |
| `08_variant_differences_and_source_caveats.md` | ADS1113/ADS1114/ADS1115 differences plus facts not documented or ambiguous in the checked-in PDF. |

## Source documents

| Source PDF | Pages used | Notes |
| --- | --- | --- |
| `docs/reference/ADS111x_datasheet_revE.pdf` | 1, 3-6, 12-28, 37 | Primary source for compact notes. |
