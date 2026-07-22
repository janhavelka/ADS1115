# COM8 HIL Evidence Summary - 2026-06-22 to 2026-06-25

This compact historical record replaces prompt-era reports and very large
ignored transcripts. All physical runs used an ESP32-S2 on `COM8` with devices
at `0x48` and `0x49`. Runtime firmware identified ADS1115 `1.1.0`, short commit
`8476da2`, build 2026-06-23 11:35:29, and a dirty worktree. These results are
diagnostic evidence only and do not validate clean 1.1.0 or current 2.0 code.

## Recorded Runs

| Run | Command/plan | Recorded result |
| --- | --- | --- |
| Functional and 8-hour HIL, 2026-06-22/23 | Broad CLI/benchmark plan followed by mixed-command soak; exact invocation not retained in repository | Smoke `PASS=12 FAIL=0 EVIDENCE_REQUIRED=2`; functional/benchmark `PASS=188 FAIL=0 EVIDENCE_REQUIRED=42`; soak `28799.4 s`, 16,296 cycles, 717,010 commands, `PASS=554051 EVIDENCE_REQUIRED=162959 FAIL=0`; latency max `0.485 s`, mean `0.039 s` |
| Targeted HIL, 2026-06-23 | `python tools/run_i2c_hil.py --port COM8 --baud 115200 --address 0x48 --address 0x49 --suite targeted --timeout-s 8 --idle-s 0.5 --boot-settle-s 2 --stop-on-fail` | `PASS=154 FAIL=0 EVIDENCE_REQUIRED=24`; contract `PASS`; evidence verdict `EVIDENCE_REQUIRED` |
| Intensive soak, 2026-06-24/25 | `--suite exhaustive --benchmark`, requested soak `72000 s` | `71998.6 s`, 40,693 cycles, 1,790,466 commands; soak `PASS=1383541 EVIDENCE_REQUIRED=406925 FAIL=0`; plan `PASS=189 EVIDENCE_REQUIRED=42 FAIL=0`; contract `PASS`; evidence verdict `EVIDENCE_REQUIRED`; latency max `0.406 s`, mean `0.039 s` |

The targeted run covered address selection, probe/settings/health/recovery,
rates, gains, MUX paths, single-shot and continuous diagnostics, comparator and
register controls, dirty-state reporting, bounded staged jobs and BUSY
interleaving, short stress, and malformed-input rejection. These are
CLI-observable digital/API checks, not calibrated analog or electrical proof.

## Removed Artifact Manifest

The intensive-soak artifacts were local ignored files and had already been
deleted before this cleanup. Their recorded metadata is retained, but their
contents cannot be independently inspected from this repository.

| Artifact | Former path | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Raw transcript | `hil_logs/ads1115_20h_soak_20260624_045300_retry2/ads1115_hil_20260624_045300.log` | 595,223,364 | `1187EEE410B0700FF5CE6F5888924B877258920B7951F580A9ACC536B894B622` |
| Generated summary | `hil_logs/ads1115_20h_soak_20260624_045300_retry2/ads1115_hil_20260624_045300_summary.md` | 54,706 | `E50C30617F10F5AF88802B22E21C2F00CBE224A8AB34487FE6723F496816C9D7` |

No retained hashes were recorded for the earlier functional, 8-hour, or
targeted artifacts. The runner summary commit recorded for the earlier broad
run was `bdd36501b129cb2309a8fbeae4da24d04618f99e`.

## Recorded Failure and Unfinished Coverage

An earlier 20-hour attempt stopped after about 14.5 hours because the host
process raised a pySerial `SerialException`; it showed `FAIL=0` only up to the
inspection point and is not a completed pass. The runner was subsequently
changed to classify serial write/read exceptions as failure rows.

These runs did not establish clean release identity, calibrated analog
accuracy, ALERT/RDY/comparator electrical behavior, physical fault recovery,
native ESP-IDF hardware behavior, ESP32-S3 coverage, or final-board acceptance.
All remain outside this historical evidence.
