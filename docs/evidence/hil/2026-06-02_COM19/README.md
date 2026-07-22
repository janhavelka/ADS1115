# COM19 HIL Evidence Summary - 2026-06-02

This is a condensed historical record of the corrected address-restore HIL
run. It is limited evidence for the attached setup and predates ADS1115 2.0.

## Identity and Setup

| Field | Recorded value |
| --- | --- |
| Run | 2026-06-02 20:52:01 to 20:52:40 |
| Branch / host helper commit | `hardening/ads1115-industry-standard-p0` / `e32822341e251a821febe4710a6879f1aff08312` |
| Firmware identity | Library `1.0.0`; short commit `9551bee`, clean; build 2026-06-02 20:47:40 |
| Controller / port | ESP32-S2-class board (`ESP32-S2FH4` reported during an earlier upload) / `COM19`, 115200 baud |
| Devices observed | `0x48` and `0x49` present; `0x4A` and `0x4B` expected absent |
| Unrecorded setup data | ADS1115 module/vendor/revision, VDD, I2C speed, pull-ups, instruments, photos |

The raw serial transcript formerly tracked at
`docs/evidence/hil/2026-06-02_COM19/ads1115_hil_20260602_205201.log` was 24,357
bytes with SHA-256
`EA98600338C6E737D7E97EBA05F588A36A11B4EAD56E40785292B2FE8FEA8B09`.
It was removed on 2026-07-22 after this summary was verified against it. The
original capture path was `hil_logs/ads1115_hil_20260602_205201.log`; that
ignored local artifact is not part of the repository.

## Commands and Results

Commands, in order:

```text
version; addr; state; cfg; drv; addr 0x48; probe; cfg; selftest;
stress 500; stress_mix 200; addr 0x49; probe; cfg; selftest;
addr 0x4A; probe; cfg; addr 0x48; probe; cfg; selftest;
addr 0x4B; probe; cfg; addr 0x48; probe; cfg; selftest;
stress 1000; stress_mix 200; drv
```

| Case | Recorded result |
| --- | --- |
| Initial state | Initialized `YES`, `READY`, active address `0x48`, hardware/cache dirty `NO` |
| Present addresses | `0x48` and `0x49` selected, probed, and initialized successfully |
| Expected-absent addresses | `0x4A` and `0x4B` returned `I2C_ERROR`, detail `0`, `I2C read length mismatch` |
| Failed selection state | Initialized address remained unchanged; requested address and last selection error remained visible |
| Restore checks | Restoring `0x48` after each negative check allowed probe/config/selftest to pass |
| Safe selftests | Each recorded selftest reported `pass=29 fail=0 skip=0` |
| Short stress | `stress 500`: 500/500; later `stress 1000`: 1000/1000; no errors |
| Mixed stress | Both `stress_mix 200` runs reported `ok=200 fail=0` and baseline restore `OK` |
| Final health | `READY`, online, consecutive failures `0`, total failures `0`, last error `never` |

## Interpretation and Unfinished Coverage

The Arduino transport returned zero bytes during the read phase for `0x4A` and
`0x4B`. It could not distinguish address NACK, timeout, bus fault, or another
read failure, so the negative checks establish only expected absence plus
successful restoration, not precise address-NACK mapping.

This run did not cover calibrated MUX/PGA/data-rate measurements, ALERT/RDY or
comparator captures, stuck-bus/unplug/brownout/partial-write faults, a 24-hour
soak, two-hour 860 SPS stress, native ESP-IDF hardware, ESP32-S3, or complete
setup metadata. It must not be used as current 2.0 release evidence.
