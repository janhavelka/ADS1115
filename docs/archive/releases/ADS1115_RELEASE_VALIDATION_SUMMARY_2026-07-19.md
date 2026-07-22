# ADS1115 2.0 Release Validation Summary - 2026-07-19

> Historical release record. Completed checks and then-current gaps are kept
> for traceability; use [`../../OPEN_ITEMS.md`](../../OPEN_ITEMS.md) for the
> current unfinished-work list.

## Outcome

The 2.0 software hardening passed its recorded native, guard, documentation,
package, and Arduino build checks. It did not include a current physical HIL
run or native ESP-IDF hardware run. Therefore this record supports the 2.0
software/API release, not a production-grade hardware or final-product claim.

The owner-safe implementation validated here is commit
`5c1ee185158ef8593cee3d1c54bea4e90880205b`. The subsequently published release
is `v2.0.0` at `785515a1683c51707169a5de854a1faafa37aa0d` (2026-07-22).
The pre-2.0 COM19/COM8 captures are indexed separately under
[`../../evidence/hil/README.md`](../../evidence/hil/README.md) and do not
validate 2.0.

## Scope Established by 2.0

- Framework-neutral, transport-injected, non-owning core; applications retain
  bus, pin, clock, timeout, locking, recovery, and task ownership.
- Fixed-memory `bind()` / `start*()` / `poll()` / `takeResult()` / `unbind()`
  owner path. `poll()` is the only owner-safe transport entry point and clamps
  callback work to three transactions per call.
- Explicit tokened result ownership, conservative deadlines, cancellation and
  quiet-time reconciliation, and bus-silent start/cancel/unbind operations.
- Explicit configuration trust and dirty/partial-hardware diagnostics until a
  complete verified replay succeeds.
- Verified fallible shutdown; `end()`/`unbind()` remain bus-silent.
- Production owner reads use single-shot OS polling. Continuous latest-register
  reads and ALERT/RDY GPIO behavior remain diagnostic contracts.
- Instances are externally serialized, non-thread-safe, non-ISR-safe, and
  non-copyable/non-movable.

## Recorded Verification

Validation date was 2026-07-19. PlatformIO Core was `6.1.19`; Doxygen was
`1.15.0`. Native tests were pinned to `1.2.1`, pioarduino espressif32 to the
exact `54.03.20` archive, and ESP-IDF CI to the documented v5.3.5 image digest.

| Check | Recorded result |
| --- | --- |
| `python -m platformio test -e native` | PASS, 174/174 |
| Core timing, CLI, IDF-example, and version guard scripts | PASS |
| `python tools/run_i2c_hil.py --parser-test` | PASS; parser only |
| Targeted `0x48`/`0x49` HIL dry-run | PASS as dry-run; 180 unique planned steps, no hardware evidence |
| Doxygen generation | PASS, no warnings |
| PlatformIO package | PASS; 120,428-byte archive inspected, then removed |
| Independent final integration review | GO; no high/medium software blocker |
| [Release CI #63](https://github.com/janhavelka/ADS1115/actions/runs/29906269941), 2026-07-22 | PASS; native tests, library validation, four Arduino builds, and both ESP-IDF target builds completed |

| Arduino environment | Flash | RAM | Recorded result |
| --- | ---: | ---: | --- |
| Diagnostic ESP32-S3 (`esp32s3dev`) | 412,574 B | 22,560 B | PASS |
| Diagnostic ESP32-S2 (`esp32s2dev`) | 405,209 B | 37,008 B | PASS |
| Owner-safe ESP32-S3 (`owner_safe_s3`) | 374,538 B | 22,616 B | PASS |
| Owner-safe ESP32-S2 (`owner_safe_s2`) | 346,309 B | 36,496 B | PASS |

`idf.py` was unavailable on the original validation host. The subsequent
release CI run completed both native ESP-IDF target builds; no physical
ESP-IDF hardware run was performed.

## Unfinished External Gates

Only unfinished validation work is listed here:

1. Run clean `v2.0.0` physical HIL on the required ESP32-S2 and ESP32-S3
   hardware. Parser tests and dry-runs are not hardware evidence.
2. Complete final-board electrical and calibrated analog validation: address
   strap, supply, pull-ups, protection, source impedance, absolute input
   limits, accuracy, saturation, disconnect behavior, and calibration.
3. Capture ALERT/RDY and comparator behavior electrically, including timing,
   polarity, latch, queue depth, and traditional/window modes.
4. Validate physical faults and the shared-bus workload: contention,
   unplug/replug, stuck lines, brownout, delayed/ambiguous transfers,
   cancellation/reconciliation, recovery, and production task cadence.
5. Run the native ESP-IDF example on physical ESP32-S2 and ESP32-S3 targets.
6. Resolve product acceptance decisions outside this repository: ADC role,
   board/profile, channel meanings, units, calibration, capacity, and required
   versus optional operation.

Use [`../../ADS1115_HARDWARE_VALIDATION_PLAN.md`](../../ADS1115_HARDWARE_VALIDATION_PLAN.md)
and the results template for new dated evidence. Do not promote the historical
pre-2.0 captures into current release evidence.
