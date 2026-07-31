# ADS1115 Driver Library

Framework-neutral ADS1115 16-bit ADC driver for ESP32-S2 and ESP32-S3 Arduino
and ESP-IDF consumers. The library never owns the I2C bus: applications inject
transport callbacks and retain ownership of handles, pins, locking, clock rate,
timeouts, recovery, retries, and scheduling.

Version 2.0 introduces a fixed-memory, owner-safe operation engine for
production shared-bus use. The native suite contains 178 fault-injection and
contract tests, and CI builds four Arduino environments plus native ESP-IDF for
ESP32-S2/S3. A clean two-device ESP32-S2 diagnostic campaign and one-hour soak
recorded zero digital-contract failures. Calibrated analog,
electrical, injected-fault, native ESP-IDF/ESP32-S3, and product-integration
work that is still open is tracked in
[`docs/OPEN_ITEMS.md`](docs/OPEN_ITEMS.md).

## Production Contract

Use this lifecycle in a serialized I2C owner task:

```text
bind()                         zero I2C
startInitialize(...)          zero I2C
poll(now, transactionBudget)  only owner-safe call that performs I2C
takeResult(token, result)     zero I2C, exactly once

startRead(...)                zero I2C
poll(...)                     bounded start / wait / verify / read steps
takeResult(...)               atomic sample plus provenance

startShutdown(...)            explicit hardware-idle operation
unbind()                      zero I2C
```

The owner-safe API provides:

- one active hardware operation and one unconsumed terminal result;
- nonzero operation tokens and exactly-once result consumption;
- absolute, wrap-safe whole-operation deadlines;
- a caller-selected callback budget per `poll()` call, clamped to three;
- per-transfer timeout clamping to the remaining operation deadline;
- mandatory profile readback before configuration becomes `VERIFIED`;
- bus-silent cancellation with post-start wait-idle reconciliation;
- fixed-memory `SampleResult` provenance and deterministically rounded nominal
  ADC-input microvolts;
- passive health diagnostics that never suppress owner-authorized I2C.

All driver calls require external serialization. No public API is ISR-safe.

## Features

- No Arduino, Wire, ESP-IDF, FreeRTOS, logging, heap, or global-bus dependency
  in `include/` or `src/`
- Injected, non-owning I2C transport with explicit callback timeouts
- Four legal address straps: `0x48`/GND, `0x49`/VDD, `0x4A`/SDA, `0x4B`/SCL
- Four single-ended and four differential MUX selections
- Six PGA ranges from +/-6.144 V through +/-0.256 V
- Eight data rates from 8 SPS through 860 SPS
- Correct signed two's-complement conversion handling
- Single-shot production acquisition; continuous mode retained as an advanced
  diagnostic surface
- Typed comparator-disabled, threshold, and conversion-ready profiles
- Partial/ambiguous write diagnostics and explicit configuration trust state
- Native ESP-IDF component metadata and diagnostic example

## Installation

The library requires C++17. Pin production dependencies to an approved tag or
full commit instead of tracking a moving branch. A tagged PlatformIO dependency
is:

```ini
lib_deps =
  https://github.com/janhavelka/ADS1115.git#v2.0.0
```

For ESP-IDF, place the repository under the application's `components/`
directory and pin it, for example with a submodule checked out at `v2.0.0`.
The native component example is under `examples/esp_idf/basic`. A source-vendored
installation must preserve both `include/ADS1115/` and `src/`.

## Transport Adapter Contract

Transport/context lifetime, bus handles, pins, locking, clock rate, retries,
recovery, and scheduling remain application-owned. Each callback invocation
must represent one bounded physical transfer attempt: acquire the external
lock, perform the transfer, and release the lock within the supplied timeout.
Do not hide retry, recovery, or multiple transfer attempts inside a callback;
an ambiguous mutating write must remain observable to the driver.

Callback buffers are valid only for the call. Preserve meaningful transport
failures in `Status` and native detail codes in `Status::detail`. Report address
or data NACK only when the transport can prove the phase. A generic NACK after
a potentially mutating write is not definite address absence. All callbacks
and driver calls require serialized task context and are not ISR-safe.

## Owner-Safe Quick Start

Transport callbacks must serialize the shared bus and honor the supplied
timeout. The complete Arduino implementation is in
`examples/02_owner_safe_poll/main.cpp`; its owner loop advances at most one I2C
callback per pass.

```cpp
#include "ADS1115/ADS1115.h"

ADS1115::ADS1115 adc;
ADS1115::OperationToken token;
bool initializationPending = false;

ADS1115::DriverConfig transport{
    appI2cWrite, appI2cWriteRead, &sharedBusOwner, 20};

ADS1115::DeviceProfile profile;
profile.i2cAddress = 0x48;
profile.defaultMux = ADS1115::Mux::AIN0_GND;
profile.defaultGain = ADS1115::Gain::FSR_2_048V;
profile.dataRate = ADS1115::DataRate::SPS_128;
profile.mode = ADS1115::Mode::SINGLE_SHOT;
profile.comparator.use = ADS1115::ComparatorUse::OFF;

ADS1115::Status st = adc.bind(transport, profile); // no I2C
if (st.ok()) {
  const uint32_t nowMs = appNowMs();
  st = adc.startInitialize(nowMs, nowMs + 200U, token); // no I2C
  initializationPending = st.inProgress();
}

// On each serialized owner-loop pass while initializationPending:
if (initializationPending) {
  ADS1115::PollResult progress = adc.poll(appNowMs(), 1);
  // done is authoritative. During reconciliation, status may be an error while
  // done remains false and the owner must continue bus-silent polling.
  if (progress.done) {
    ADS1115::OperationResult result;
    ADS1115::Status access = adc.takeResult(token, result);
    initializationPending = false;
    if (access.ok() && result.status.ok()) {
      // Initialization completed with verified, clean configuration.
    }
  }
}
```

Schedule a read only after initialization succeeds:

```cpp
ADS1115::ChannelRequest request;
request.channelId = 7; // application meaning, preserved in SampleResult
request.mux = ADS1115::Mux::AIN2_GND;
request.gain = ADS1115::Gain::FSR_1_024V;

const uint32_t nowMs = appNowMs();
const uint32_t durationMs =
    ADS1115::operationDeadlineMs(1, profile.dataRate,
                                 3 * transport.transferTimeoutMs + 5);
st = adc.startRead(request, nowMs, nowMs + durationMs, token);
```

`operationDeadlineMs()` returns a duration, not an absolute timestamp. It uses
the datasheet's -10% data-rate tolerance plus a one-millisecond conversion
guard and the caller's scheduling margin. For a read, that margin must cover
the three possible callback runtimes plus owner scheduling jitter; the helper
cannot infer application transport timing from a `DataRate` alone.

## Profiles And Configuration Trust

`DriverConfig` contains only the non-owning transport binding and transfer cap.
`DeviceProfile` is the complete desired volatile hardware profile: address,
default MUX/gain, data rate, mode, and comparator configuration.
`ChannelRequest` records the application channel ID, MUX, and gain for one
single-shot conversion.

Owner initialization and recovery always perform:

1. CONFIG-register reachability probe;
2. low-threshold, high-threshold, and CONFIG writes;
3. low-threshold, high-threshold, and masked CONFIG readback.

ADS1115 has no chip-ID register. This proves address reachability and register
profile plausibility, not silicon identity. The dynamic CONFIG OS bit is masked.

Configuration state is explicit:

| State | Meaning |
| --- | --- |
| `UNBOUND` | No transport/profile binding exists. |
| `UNCONFIGURED` | Bound, but hardware has not been verified. |
| `APPLYING` | A profile-changing owner operation is active. |
| `VERIFIED` | All writable profile fields matched readback. |
| `UNKNOWN` | Hardware/cache agreement cannot safely label or scale a sample. |

Profile generation increments after each verified profile commit and after a
read-specific MUX/PGA CONFIG is verified ready. Raw writes,
ambiguous/partial writes, post-effect cancellation, and operation timeout can
move the state to `UNKNOWN`. `startRead()` rejects unverified or dirty state;
use a successful `startRecover()` operation to replay and verify the profile.
`startApplyProfile()` cannot change the bound I2C address; use a quiescent
`unbind()` followed by `bind()` for an address change.

## Operations, Budgets, And Results

Every `start*()` call is bus-silent and returns `Err::IN_PROGRESS` plus a token
when accepted. `poll(nowMs, maxTransactions)` is the only owner-safe call that
touches I2C. A budget of zero is bus-silent; values above three are clamped.
Conversion-time wait polls consume zero callbacks.

| Operation | Callback sequence | Maximum callbacks |
| --- | --- | ---: |
| Initialize | probe, 3 writes, 3 readbacks | 7 |
| Apply profile | 3 writes, 3 readbacks | 6 |
| Recover | tracked probe, 3 writes, 3 readbacks | 7 |
| Single-shot read | start CONFIG write, masked CONFIG verification, conversion read | 3 |
| Shutdown | single-shot CONFIG write and readback | 2 |

Callback timeout caps are partitioned across the requested poll budget so their
sum cannot exceed `deadline - nowMs` as sampled at the poll boundary. Every cap
is also limited by `transferTimeoutMs`. The owner must pass `nowMs` from the same
monotonic time domain used at start. Deadlines must be in the future by at most
`INT32_MAX` milliseconds.

When a poll budget allows multiple callbacks, `nowMs` is sampled only at the
poll boundary and the remaining timeout is divided conservatively between the
callbacks. Use a budget of one when the system requires one-transfer scheduling
or wants to make unused callback time available to a later scheduler cycle.

Size every operation deadline for all possible callbacks in the table plus
owner scheduling jitter. `operationDeadlineMs()` helps with conversion time,
but the application must add callback and scheduling allowance for initialize,
apply, recover, read, and shutdown operations.

`PollResult::done` is the terminal indicator. In reconciliation,
`PollResult::status` can already carry a preserved transport failure while
`done` remains false; keep polling until terminal, then call `takeResult()` and
inspect `OperationResult::status`.

When `PollResult::done` becomes true, call `takeResult()` with the matching
token. A wrong token returns `TOKEN_MISMATCH` without consuming the result.
Reading twice returns `RESULT_NOT_AVAILABLE`. A new operation remains blocked
until the pending terminal result is consumed.

`OperationResult::status` is the operation outcome. A successful read sets
`sampleValid` and publishes one `SampleResult` containing:

- raw signed code and rounded ADC-input microvolts;
- application channel ID, MUX, gain, and data rate;
- configuration generation and monotonic successful-sample sequence;
- verified-configuration and positive/negative code-limit flags.

The result is a value object. No pointer into mutable driver storage is exposed.
It contains a sequence and configuration provenance, but no timestamp,
freshness, or board-level validity. The application must attach its chosen
capture/completion timestamp and freshness policy.

## Cancellation And Timeout Safety

`cancelActiveOperation()` never performs I2C.

- Before any hardware effect, cancellation publishes `CANCELLED` immediately.
- While a confirmed or ambiguous conversion may still be active, cancellation
  enters `RECONCILING`. Subsequent polls remain bus-silent until the proven
  worst-case conversion interval has elapsed. Once OS/readback has proved the
  conversion idle, cancellation can terminate without that quiet wait.
- New reads and configuration mutations remain blocked during reconciliation.
- The abandoned conversion is never published or reused for another MUX.
- If the start callback already returned an ambiguous transport failure, that
  original failure remains the terminal result.

A deadline reached while the conversion may still be active follows the same
wait-idle rule and publishes `TIMED_OUT`. Cancelled or timed-out reads before
readiness verification, and profile-changing cancellations/timeouts, retain
`UNKNOWN`/dirty diagnostics when hardware may have changed; recover before the
next typed read.

## Units And Pure Helpers

`rawToMicrovolts()` uses bounded 64-bit integer arithmetic and rounded rational
scaling. `SampleResult::microvolts` is voltage at the selected ADS1115 input,
not a board-level engineering unit. Divider ratio, shunt value, amplifier gain,
offset, calibration, polarity, protection, and disconnect semantics belong to
the board/product layer.

Other bus-silent helpers include:

- `validateDeviceProfile()`, `validateComparatorProfile()`, and
  `validateChannelRequest()`;
- `dataRateSps()` and `worstCaseConversionTimeUs()`;
- `gainFullScaleMicrovolts()`;
- `isSingleEnded()`, `positiveInput()`, and `negativeInput()`;
- `operationDeadlineMs()`.

## Comparator And ALERT/RDY

`ComparatorProfile::use` distinguishes disabled output, threshold comparison,
and the datasheet conversion-ready threshold pattern. Invalid combinations and
threshold ordering are rejected before I2C. Comparator thresholds are signed
raw ADC codes and must be recalculated when PGA changes.

The production owner-safe read path uses CONFIG OS-bit polling. It does not own
or sample a GPIO. Legacy ALERT/RDY support remains an advanced diagnostic path.
ALERT/RDY is open drain and needs a board-selected pull-up. Conversion-ready
pulses can be short (approximately 8 us in continuous mode), so a reviewed GPIO,
edge/latch policy, and electrical validation are required before product use.

## Health And Fault Ownership

`READY`, `DEGRADED`, and `OFFLINE` summarize consecutive tracked transport
results. Counters saturate, a success resets consecutive failures, and timestamps
use the compatibility clock hook when available.

Health is passive. `OFFLINE` never blocks a callback selected by the external
owner and never starts retries or bus recovery. The application remains the only
owner of admission, retry, backoff, reset, and recovery policy.
Likewise, `DriverState::READY` alone is not proof that a sample is publishable.
Require successful terminal completion, `sampleValid`, and verified clean
configuration provenance.
Tracked callbacks executed by owner operations timestamp health with that
operation's current `poll(nowMs, ...)` value; legacy direct calls use the
optional `Config::nowMs` hook.

Multi-register writes can partially reach hardware. The driver preserves the
original transport/readback error through `hardwareConfigDirtyError()` and the
address through `SettingsSnapshot::hardwareConfigDirtyAddress`. Only a complete
verified replay clears the dirty state.

## Migrating From 1.x And Advanced Diagnostics

The 1.x surface remains available for migration, bring-up, and service tools,
but its behavior has intentionally hardened in 2.0:

| Surface | 2.0 contract |
| --- | --- |
| `Config` / `begin()` | Synchronous compatibility facade; initialization now always performs full readback. |
| `recover()` / `shutdown()` | Bounded synchronous facades over the same engine; can perform 7 / 2 callbacks. |
| `end()` | Bus-silent alias for `unbind()`; call `shutdown()` explicitly when hardware idle is required. |
| Direct setters | Advanced diagnostics; a successful direct mutation makes profile state `UNKNOWN` until verified replay. |
| Raw register writes | Advanced diagnostics; always mark cache/hardware state dirty or preserve an ambiguous write failure. |
| `conversionReady()` bool overload | Lossy compatibility only; `false` conflates not-ready and error. |
| Blocking reads | Bounded compatibility convenience requiring `Config::nowMs`; single-shot verified profiles only. |
| Continuous mode | Latest-register diagnostics only; owner-safe `startRead()` deliberately rejects it. |
| Legacy staged jobs | Compatibility wrappers over the owner operation engine. |

Status enum values from 1.x retain their numeric values. New 2.0 values are
appended: `CANCELLED`, `CONFIG_UNKNOWN`, `RESULT_NOT_AVAILABLE`,
`TOKEN_MISMATCH`, and `INDETERMINATE`. This release is a major version because
lifecycle, verification, shutdown, health-admission, continuous-read, and
configuration-trust behavior changed.

## Examples

| Example | Intent | Limits |
| --- | --- | --- |
| `examples/02_owner_safe_poll/` | Production ownership pattern: static mutex, shared-bus timeout enforcement, tokened operations, and one callback per scheduler pass. | Example pins and sample channel meaning must be replaced by the product profile. |
| `examples/01_basic_bringup_cli/` | The diagnostic Arduino bring-up CLI and HIL console. | Uses the compatibility and raw diagnostic surface; it is not a production bus manager. |
| `examples/esp_idf/basic/` | Native IDF `app_main`, `i2c_master`, fixed-buffer diagnostic CLI, `esp_timer`, and FreeRTOS integration. | Externally serialized demo with coarse `esp_err_t` mapping; it does not include a shared-bus mutex. |

`examples/common/` is example-only glue and is not part of the library.

## Resource, Threading, And Electrical Contracts

1. Driver instances are neither thread-safe nor ISR-safe; serialize every call.
2. The application owns transport lifetime. Do not destroy/reconfigure the bus
   context while the driver remains bound.
3. The core allocates no heap memory and has no unbounded wait, retry, or queue.
4. A transport callback may block only up to its supplied timeout and must
   return a meaningful `Status`.
5. `unbind()` and `end()` are always bus-silent; shutdown is an explicit fallible
   operation.
   Cancel and finish wait-idle reconciliation before unbinding an active
   conversion; otherwise the caller must enforce the same worst-case quiet
   interval before reusing that physical device.
6. PGA full-scale selection does not change ADS1115 absolute input limits.
   Keep analog pins within the powered-device datasheet limits.
7. The ADDR pin is continuously sampled. Board strap choice and I2C/ALERT pull-up
   sizing are electrical design inputs, not driver policy.

## Validation And Reproducibility

The repository pins PlatformIO Core `6.1.19`, PlatformIO Native `1.2.1`, and
the exact pioarduino espressif32 `55.03.311` release archive. That Arduino
platform supplies Arduino-ESP32 `3.3.11` and bundled ESP-IDF `5.5.5`. The
native ESP-IDF CI build is a separate compatibility baseline pinned to
ESP-IDF `v5.3.5`. CI action revisions, runner images, Doxygen, and the host
compiler still vary over time; builds are not claimed bit-reproducible.

On Windows hosts with Win32 long-path support disabled, the Arduino `3.3.11`
libraries archive can exceed the default extraction path limit. Enable Win32
long paths or set `PLATFORMIO_CORE_DIR` to a short, application-owned directory
before the first package installation.

Configured CI runs:

```bash
python tools/check_core_timing_guard.py
python scripts/generate_version.py check
python -m platformio test -e native
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/run_i2c_hil.py --parser-test
python tools/run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted
doxygen Doxyfile
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio run -e owner_safe_s3
python -m platformio run -e owner_safe_s2
python -m platformio pkg pack
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Do not report an unrun build or hardware case as passed. COM19/COM8 evidence
predates 2.0. The clean 2026-07-22 COM6 campaign is a prior-platform baseline;
it was recorded before the `55.03.311` upgrade and does not qualify the current
Arduino stack. It covers the diagnostic surface on ESP32-S2 at addresses
`0x48` and `0x49`, absent-address checks at `0x4A` and `0x4B`, and a
3,600-second soak; it does not prove calibrated analog, ALERT/RDY electrical,
injected-fault, ESP-IDF/ESP32-S3, or final-product behavior. The diagnostic
`version` output now reports Arduino-ESP32 and ESP-IDF versions, and the HIL
runner rejects firmware that does not report `3.3.11` / `v5.5.5`. See the compact
<a href="docs/evidence/hil/2026-07-22_COM6/README.md">COM6 evidence summary</a>.
The unfinished gates remain in [`docs/OPEN_ITEMS.md`](docs/OPEN_ITEMS.md);
execution details belong in the hardware validation plan.

## TunnelMonitor-node Integration Boundary

The 2.0 owner API is ready for an adapter, but no TunnelMonitor firmware
contract was changed. The unfinished product, board/profile, adapter, capacity,
and final-board gates are consolidated in
[`docs/OPEN_ITEMS.md`](docs/OPEN_ITEMS.md). The recommended initial scope is a
fixed compile-time profile, single-shot reads with OS-bit polling,
comparator/ALERT-RDY disabled, and one I2C callback per owner-task poll.

## Documentation

- `CHANGELOG.md` - release history and 2.0 migration notes
- `docs/OPEN_ITEMS.md` - single index of unfinished release and integration work
- `docs/IDF_PORT.md` - ESP-IDF adapter and error-mapping guidance
- `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md` - hardware evidence procedure
- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md` - dated capture template
- `docs/evidence/hil/README.md` - compact index of dated fixture evidence
- `docs/reference/extracted-md/` - retained datasheet-derived transcripts for
  quick human and AI-assisted contract review
- `docs/README.md` - current documentation index
- `Doxyfile` - warning-enforced generated public API reference

## License

MIT License. See `LICENSE`.
