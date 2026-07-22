# COM19 HIL Summary - 2026-06-02

Historical pre-2.0 diagnostic evidence only.

| Item | Result |
| --- | --- |
| Firmware / fixture | ADS1115 1.0.0, clean `9551bee`; ESP32-S2-class board on COM19 |
| Addresses | `0x48` and `0x49` passed; expected-absent `0x4A`/`0x4B` returned a generic read error and restored cleanly to `0x48` |
| Self-test | Repeated `29 pass, 0 fail, 0 skip` |
| Stress | 500/500 and 1,000/1,000 scalar reads; two mixed 200/200 runs |
| Final health | READY, zero consecutive or total failures |

The transport could not distinguish address NACK from another read failure.
No calibrated analog, electrical, physical-fault, native ESP-IDF, ESP32-S3, or
complete fixture evidence was captured. Full serial transcripts were deleted;
this compact result is the only retained record.
