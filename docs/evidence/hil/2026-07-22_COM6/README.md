# ESP32-S2 COM6 HIL - 2026-07-22

## Outcome

The clean v2.0.0 firmware at commit
`dd25d616d2efd0ab6ce2aa54bfa56d74cbb4237c` passed the complete digital
contract on two responding ADS1115 addresses, a continuous one-hour soak, and
a post-soak smoke test. No command failure, unexpected reset, or reboot marker
was observed. Analog accuracy and electrical fault behavior remain open because
this fixture had no calibrated source, meter, scope, or fault injector.

## Fixture

| Item | Value |
| --- | --- |
| Date and time zone | 2026-07-22, CEST |
| Controller | ESP32-S2FH4 revision 1.0, 4 MB flash |
| Controller identity | MAC `90:e5:b1:8b:1d:60`; USB VID:PID `303A:0002`; COM6 |
| Firmware | ADS1115 2.0.0, clean commit `dd25d616d2efd0ab6ce2aa54bfa56d74cbb4237c` |
| I2C configuration | SDA GPIO8, SCL GPIO9, 400 kHz, 50 ms adapter timeout |
| Responding addresses | `0x48`, `0x49` |
| Negative address checks | `0x4A`, `0x4B` did not acknowledge |
| ALERT/RDY | Not connected (`ALERT_RDY_PIN = -1`) |
| External instruments | None |

The ADS1115 has no chip-ID register. Address detection and initialization are
CONFIG plausibility/read-back checks, not silicon identity proof.

## Commands and results

The principal run used:

```text
python tools/run_i2c_hil.py --port COM6 --baud 115200 --address 0x48 --address 0x49 --absent-address 0x4A --absent-address 0x4B --suite exhaustive --benchmark --timeout-s 12 --idle-s 0.35 --boot-settle-s 1 --stop-on-fail --soak --soak-duration-s 3600 --soak-max-consecutive-failures 1 --out hil_logs/2026-07-22_COM6_exhaustive_1h_dd25d61
```

| Phase | Result |
| --- | --- |
| Targeted preflight | 161 PASS, 0 FAIL, 22 EVIDENCE_REQUIRED |
| Exhaustive prerequisite | 455 steps: 391 PASS, 0 FAIL, 64 EVIDENCE_REQUIRED |
| One-hour soak | 3600.123 s; 2,095 cycles; 87,956 commands; 71,200 PASS; 0 FAIL; 16,756 EVIDENCE_REQUIRED |
| Soak timing and stability | 0.039 s mean, 0.183 s worst command/read, 0 consecutive failures, 0 unexpected resets/reboots |
| Post-soak smoke | 15 PASS, 0 FAIL, 2 EVIDENCE_REQUIRED |
| Final classified verdict | Digital contract PASS; physical evidence still required |

`EVIDENCE_REQUIRED` marks observations that a serial transcript alone cannot
prove, such as analog accuracy or electrical pin behavior. It is not a hidden
command failure.

Coverage included all eight MUX selections, six PGA ranges, eight data rates,
single-shot and continuous operation, comparator modes/polarities/latching/
queues and threshold extremes, conversion-ready configuration, all readable
registers and writable diagnostic registers, dirty-state recovery, staged job
budgets/BUSY/cancellation/profile application/shutdown, invalid parameter
boundaries, absent-address behavior, benchmarks, and stress loops.

## Hard issues found and fixed

- The example loop could advance explicitly staged diagnostic jobs in the
  background. Arduino and native ESP-IDF examples now leave those jobs under
  explicit `job poll` ownership.
- Manual serial jobs inherited scheduler-sized deadlines that serial round
  trips could consume. Diagnostic commands now start owner operations with an
  explicit finite 5 s operator deadline while retaining the production owner
  API and transaction budgets.
- The host gate previously had incomplete exhaustive composition and weak
  prerequisite, status, self-test, scan, reset, and soak-failure checks. These
  were consolidated into the classified runner.
- Four owner lifecycle/fault regressions were added; all 178 native tests pass.

No core driver defect remained after these fixes; changes were confined to
diagnostic scheduling, validation, tests, and documentation.

## Artifact identities

Raw captures remain locally under ignored `hil_logs/`; only this concise record
is versioned. The hashes identify the exact captures but do not constitute an
external immutable archive. The tested application image was read back from
flash after the run and matched its pre-test build hash exactly.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `hil_logs/2026-07-22_COM6_targeted_dd25d61/ads1115_hil_20260722_174846.log` | 50,477 | `EA5251513046B882978ECAB05A9920879716CDDB87204C9A27ACCD5563A7EB48` |
| `hil_logs/2026-07-22_COM6_targeted_dd25d61/ads1115_hil_20260722_174846_summary.md` | 44,669 | `01134CE2BD8EA7F65BD1A8BE0D19EEFE349487D6F13EEAF2C668AE24617A26D6` |
| `hil_logs/2026-07-22_COM6_exhaustive_1h_dd25d61/ads1115_hil_20260722_174917.log` | 29,839,858 | `0CB68D5DC037454AD14BDBB21F53924B8EA3F97073FA18638FFA107E2EAD40B5` |
| `hil_logs/2026-07-22_COM6_exhaustive_1h_dd25d61/ads1115_hil_20260722_174917_summary.md` | 109,226 | `B7BF3C178417097C84D53541A3A78C5FA5DA31A9635EB274E5C7C3F538EBF4A2` |
| `hil_logs/2026-07-22_COM6_postsoak_smoke/ads1115_hil_20260722_185028.log` | 10,945 | `66F47C51E7FC9A3A24071BFB766351FDC279ECFCFA9676EB07E5270B75981852` |
| `hil_logs/2026-07-22_COM6_postsoak_smoke/ads1115_hil_20260722_185028_summary.md` | 6,417 | `408B6AE6972B6823BFE6E44A130B0F6CC2FE0AFAEC30422D508981FE2D07FD9D` |
| `hil_logs/2026-07-22_COM6_exhaustive_1h_dd25d61/firmware_dd25d61_readback.bin` | 405,760 | `1998CE70AC14036C845C81ECB2489E779CEDE8A83304F6261C095D72B23E089A` |

## Remaining limits

- No calibrated reference or DMM was available, so accuracy, linearity,
  calibration, clipping, source impedance, protection, and physical
  disconnection behavior are unproven.
- Comparator and conversion-ready register behavior was covered, but no
  ALERT/RDY electrical level or timing was captured.
- No physical contention, unplug/replug, stuck bus, brownout, or ambiguous
  transfer was injected. Native fault injection and absent-address checks are
  complementary, not substitutes.
- This was Arduino diagnostic HIL on ESP32-S2. It does not establish native
  ESP-IDF or ESP32-S3 hardware behavior.
- `0x48` and `0x49` were populated. `0x4A` and `0x4B` prove negative-address
  handling only, not operation with those strap configurations populated.

## TunnelMonitor-node relevance

This run establishes a clean library and diagnostic baseline. It does not
validate the future TunnelMonitor-node module, its ESP32-S3 final board, native
ESP-IDF adapter, shared-bus locking/fairness, queue and deadline behavior, task
cadence, timestamp provenance, or final analog channels. Those product-level
items remain explicit acceptance gates.
