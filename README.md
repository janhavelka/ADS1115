# ADS1115 Driver Library

Framework-neutral ADS1115 16-bit ADC driver for ESP32-S2 and ESP32-S3 Arduino
and ESP-IDF consumers. The library never owns the I2C bus: applications inject
transport callbacks and retain ownership of handles, pins, locking, clock rate,
timeouts, recovery, retries, and scheduling.

Version 2.0 introduces a fixed-memory, owner-safe operation engine for
production shared-bus use. Native fault injection and ESP32 build coverage are
strong; field release still requires the board-specific electrical and HIL
evidence listed below.

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
- fixed-memory `SampleResult` provenance and exact ADC-input microvolts;
- passive health diagnostics that never suppress owner-authorized I2C.

All driver calls require external serialization. No public API is ISR-safe.

## Features

- No Arduino, Wire, ESP-IDF, FreeRTOS, logging, heap, or global-bus dependency
  in `include/` or `src/`
- Injected, non-owning I2C transport with explicit callback timeouts
- Four legal addresses: `0x48` through `0x4B`
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

For PlatformIO, add the library to `lib_deps`, or copy `include/ADS1115/` and
`src/` into the project. For ESP-IDF, use this repository as a component or the
native project under `examples/esp_idf/basic`.

The repository's reproducible build inputs are PlatformIO Core `6.1.19`, the
exact pioarduino espressif32 `54.03.20` release archive, and ESP-IDF `v5.3.5`
for the native IDF CI build.

## Owner-Safe Quick Start

Transport callbacks must serialize the shared bus and honor the supplied
timeout. The complete Arduino implementation is in
`examples/02_owner_safe_poll/main.cpp`; its owner loop advances at most one I2C
callback per pass.

```cpp
#include "ADS1115/ADS1115.h"

ADS1115::ADS1115 adc;
ADS1115::OperationToken token;

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
}

// Called only by the serialized bus owner:
ADS1115::PollResult progress = adc.poll(appNowMs(), 1);
if (progress.done) {
  ADS1115::OperationResult result;
  ADS1115::Status access = adc.takeResult(token, result);
  if (access.ok() && result.status.ok()) {
    // Initialization is verified, or result.sample is an atomic typed sample.
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
    ADS1115::operationDeadlineMs(1, profile.dataRate, 20);
st = adc.startRead(request, nowMs, nowMs + durationMs, token);
```

`operationDeadlineMs()` returns a duration, not an absolute timestamp. It uses
the datasheet's -10% data-rate tolerance plus a one-millisecond conversion
guard and the caller's scheduling margin.

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

Profile generation increments only after verified commit. Raw writes,
ambiguous/partial writes, post-effect cancellation, and operation timeout can
move the state to `UNKNOWN`. `startRead()` rejects unverified or dirty state;
use a successful `startRecover()` operation to replay and verify the profile.

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

The callback timeout is `min(transferTimeoutMs, deadline - nowMs)`. The owner
must pass `nowMs` from the same monotonic time domain used at start. Deadlines
must be in the future by at most `INT32_MAX` milliseconds.

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

## Cancellation And Timeout Safety

`cancelActiveOperation()` never performs I2C.

- Before any hardware effect, cancellation publishes `CANCELLED` immediately.
- After a confirmed or ambiguous conversion start, cancellation enters
  `RECONCILING`. Subsequent polls remain bus-silent until the proven worst-case
  conversion interval has elapsed.
- New reads and configuration mutations remain blocked during reconciliation.
- The abandoned conversion is never published or reused for another MUX.
- If the start callback already returned an ambiguous transport failure, that
  original failure remains the terminal result.

A deadline reached after conversion start follows the same wait-idle rule and
publishes `TIMED_OUT`. Profile-changing cancellations/timeouts retain
`UNKNOWN`/dirty diagnostics when hardware may have changed.

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

Multi-register writes can partially reach hardware. The driver preserves the
original transport/readback error through `hardwareConfigDirtyError()` and the
address through `SettingsSnapshot::hardwareConfigDirtyAddress`. Only a complete
verified replay clears the dirty state.

## Compatibility And Advanced Diagnostics

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
| Blocking reads | Bounded compatibility convenience; single-shot verified profiles only. |
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
| `examples/01_basic_bringup_cli/` | Arduino diagnostic/HIL console. | Uses the compatibility and raw diagnostic surface; it is not a production bus manager. |
| `examples/esp_idf/basic/` | Native IDF `app_main`, `i2c_master`, fixed-buffer diagnostic CLI, `esp_timer`, and FreeRTOS integration. | Externally serialized demo with coarse `esp_err_t` mapping; no shared-bus mutex. |

`examples/common/` is example-only glue and is not part of the library.

The diagnostic Arduino bring-up CLI and ESP-IDF CLI predate the owner-safe
example. Current examples are diagnostic except for the explicitly scoped
`02_owner_safe_poll` ownership pattern. The ESP-IDF diagnostic example
does not include a shared-bus mutex. Production applications should implement their own
board/profile meanings, shared-bus admission, and system recovery policy.
Raw writes bypass the typed config helpers and require a verified replay before
typed/scaled acquisition resumes.

## Resource, Threading, And Electrical Contracts

1. Driver instances are neither thread-safe nor ISR-safe; serialize every call.
2. The application owns transport lifetime. Do not destroy/reconfigure the bus
   context while the driver remains bound.
3. The core allocates no heap memory and has no unbounded wait, retry, or queue.
4. A transport callback may block only up to its supplied timeout and must
   return a meaningful `Status`.
5. `unbind()` and `end()` are always bus-silent; shutdown is an explicit fallible
   operation.
6. PGA full-scale selection does not change ADS1115 absolute input limits.
   Keep analog pins within the powered-device datasheet limits.
7. The ADDR pin is continuously sampled. Board strap choice and I2C/ALERT pull-up
   sizing are electrical design inputs, not driver policy.

## Validation And Reproducibility

Configured CI runs:

```bash
python tools/check_core_timing_guard.py
python scripts/generate_version.py check
python -m platformio test -e native
python tools/check_cli_contract.py
python tools/check_idf_example_contract.py
python tools/run_i2c_hil.py --parser-test
python tools/run_i2c_hil.py --dry-run --address 0x48 --address 0x49 --suite targeted
python tools/hil_ads1115_capture.py --dry-run --suite identity
python -m platformio run -e esp32s3dev
python -m platformio run -e esp32s2dev
python -m platformio run -e owner_safe_s3
python -m platformio run -e owner_safe_s2
python -m platformio pkg pack
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

If `idf.py` or hardware is unavailable locally, record that gap; do not report
a pass. Existing COM19/COM8 evidence is limited and predates 2.0.

Before field release, capture dated evidence for all four address straps used by
supported boards, all selected MUX/PGA/rate combinations, analog accuracy and
source impedance, saturation/disconnect behavior, ALERT/RDY if enabled, stuck
bus/unplug/replug/brownout, ambiguous/partial writes, shared-bus contention,
cancel/timeout reconciliation, and the final target workload.

## TunnelMonitor-node Re-audit Boundary

The 2.0 owner API resolves the library-side ownership, deadline, cancellation,
configuration-trust, sample-provenance, passive-health, and verified-init gaps
identified in `docs/TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md`.

No TunnelMonitor firmware contracts were changed. Integration remains blocked
on a product decision: ADS1115 must be defined either as a proven replacement
for the existing power-monitor meaning or as a distinct analog source. Address,
board revision, channel meanings, analog front end, calibration, engineering
units, required/optional role, capacity, and final HIL remain external gates.

## Documentation

- `CHANGELOG.md` - release history and 2.0 migration notes
- `docs/TUNNELMONITOR_NODE_SUITABILITY_AUDIT.md` - finding-by-finding evidence
  and implementation disposition
- `docs/IDF_PORT.md` - ESP-IDF adapter and error-mapping guidance
- `docs/ADS1115_HARDWARE_VALIDATION_PLAN.md` - hardware evidence procedure
- `docs/ADS1115_HARDWARE_VALIDATION_RESULTS_TEMPLATE.md` - dated capture template
- `docs/README.md` - current documentation index
- `docs/archive/` - historical audits and hardening reports

## License

MIT License. See `LICENSE`.
