# COM8 HIL Summary - 2026-06-22 to 2026-06-25

Historical diagnostic evidence from dirty ADS1115 1.1.0 firmware at `8476da2`
on an ESP32-S2 with responding devices at `0x48` and `0x49`. It is not clean
release evidence.

| Run | Result |
| --- | --- |
| Functional plus 8-hour soak | 717,010 commands; 0 classified failures; 0.485 s maximum latency |
| Targeted | 154 PASS, 0 FAIL, 24 EVIDENCE_REQUIRED |
| Intensive 20-hour soak | 1,790,466 commands; 0 classified failures; 0.406 s maximum latency |

An earlier 20-hour attempt ended after about 14.5 hours with a host serial
exception and was not counted as a pass. These runs established CLI-observable
digital behavior only, not calibrated analog, ALERT/RDY/comparator electrical,
physical-fault, native ESP-IDF, ESP32-S3, or final-board behavior. Full
transcripts and detailed runner summaries were deleted; this table is the only
retained record.
