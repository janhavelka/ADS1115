# ESP32-S2 COM6 HIL Summary - 2026-07-22

Clean post-tag ADS1115 2.0.0 firmware at
`dd25d616d2efd0ab6ce2aa54bfa56d74cbb4237c` passed the digital contract. The
run does not validate the earlier `v2.0.0` tag artifact. It also predates the
pioarduino `55.03.311` upgrade and is not qualification evidence for the
current Arduino-ESP32 `3.3.11` / ESP-IDF `5.5.5` stack.

| Item | Result |
| --- | --- |
| Fixture | ESP32-S2FH4 rev 1.0, 4 MB; SDA GPIO8, SCL GPIO9, 400 kHz, 50 ms timeout |
| Addresses | `0x48` and `0x49` responded; `0x4A` and `0x4B` were expected absent |
| Targeted preflight | 161 PASS, 0 FAIL, 22 EVIDENCE_REQUIRED |
| Exhaustive plan | 391 PASS, 0 FAIL, 64 EVIDENCE_REQUIRED |
| One-hour soak | 3,600.123 s; 2,095 cycles; 87,956 commands; 0 failures or unexpected resets |
| Latency | 0.039 s mean; 0.183 s maximum |
| Post-soak smoke | 15 PASS, 0 FAIL, 2 EVIDENCE_REQUIRED |

Coverage included every MUX, PGA, and data rate; both conversion modes;
comparator/register controls; dirty recovery; staged budgets, cancellation,
profile application and shutdown; invalid inputs; absent addresses; benchmark
and stress paths. `EVIDENCE_REQUIRED` denotes behavior needing physical
measurement, not a hidden command failure.

No calibrated analog source, DMM, scope, ALERT/RDY connection, injected bus
fault, native ESP-IDF target, ESP32-S3, or TunnelMonitor product workload was
present. ADS1115 reachability is register plausibility, not chip identity. Full
transcripts, detailed runner summaries, and the firmware readback were deleted;
this compact dated summary is the only retained HIL record.
