# ADS1115 ESP-IDF Portability Status

## Current Contract

The core is framework-neutral. `include/` and `src/` contain no Arduino,
ESP-IDF, FreeRTOS, GPIO, logging, global bus, or framework-delay dependency.
I2C is injected and non-owning.

For production shared-bus use, bind `DriverConfig` and `DeviceProfile`, schedule
tokened operations with `start*()`, and call `poll(nowMs, budget)` from the sole
I2C owner. `poll()` is the only owner-safe API that invokes transport callbacks.
Start, cancel, result consumption, `unbind()`, and `end()` are bus-silent.

The native example under `examples/esp_idf/basic` remains a diagnostic CLI. It
uses `app_main`, `driver/i2c_master.h`, `esp_timer`, FreeRTOS delays, IDF GPIO,
fixed command buffers, an external bus context, timeout propagation, and
conservative `esp_err_t` mapping. It does not include a shared-bus mutex or
system-wide retry/recovery policy.

## Production Adapter Responsibilities

The application must provide:

1. One owner for the I2C master bus/device handle.
2. External serialization across all ADS1115 calls and shared-bus clients.
3. `i2cWrite` and `i2cWriteRead` callbacks that honor the supplied timeout.
4. A monotonic millisecond timestamp passed consistently to `start*()` and
   `poll()`.
5. Owner policy for admission, retries, backoff, bus recovery, reset, and device
   health. Driver `OFFLINE` is passive diagnostics only.
6. Board-owned pins, pull-ups, clock rate, and optional ALERT/RDY handling.

Do not call driver methods from ISR context. An ALERT/RDY ISR may record a
bounded edge/timestamp only; I2C stays in the owner task. Owner `startRead()`
establishes readiness through timed CONFIG/OS polling even when the profile
enables conversion-ready output. The GPIO readiness shortcut belongs to the
legacy diagnostic surface.

## Owner Integration

Use the framework-neutral owner lifecycle and deadline sizing in the
[`README`](../README.md#owner-safe-quick-start). An ESP-IDF adapter should
range-check and pass the callback timeout to the IDF transfer, serialize the
complete write/repeated-start/read sequence, and preserve the raw `esp_err_t`
in `Status::detail`. The driver already clamps the callback cap to the remaining
whole-operation time and partitions it across the poll's transaction budget.
Lock acquisition and the complete transfer must fit that supplied cap; use one
physical attempt per callback and leave retry/recovery to the owner.

A callback must return a terminal `Status`. On writes, only a proven
`I2C_NACK_ADDR`, or adapter `INVALID_CONFIG`/`INVALID_PARAM` returned before any
transfer, establishes that nothing reached the device. Other failures retain
possible hardware effects; a pre-transfer lock timeout must not be relabeled as
an address NACK. The driver converts unsupported callback `IN_PROGRESS` to
`INDETERMINATE` and preserves its detail.

## Verification And Identity Limits

Owner initialization/recovery normally performs a CONFIG reachability read,
three writes, and three readbacks: seven callbacks. If conversion activity is
uncertain or the probe shows continuous mode or OS busy, it first writes
single-shot mode, waits the conservative 8-SPS interval (140 ms), and verifies
CONFIG and OS idle before the full replay. That path uses eight callbacks plus
idle-readiness polls, whose retries remain bounded by the operation deadline.
An initial OS-idle read cannot by itself clear retained conversion uncertainty.

Profile apply normally uses six callbacks. Tracked continuous or uncertain
conversion state adds the same probe/idle preflight, for up to eight callbacks
plus idle-readiness polls. Include the wait, all callback timeout caps, readiness
retries, and scheduler jitter in the owner deadline.

CONFIG comparison masks dynamic OS/status bits when checking writable fields,
but single-shot initialization/replay and shutdown also require a separate OS
idle check. Typed reads verify CONFIG and OS before fetching conversion data.
ADS1115 has no ID register, so success proves address/register plausibility,
not identity. Use a fixed board address inventory and final-board HIL.

Multi-register operations can partially reach hardware. The error from the
latest hardware-affecting step remains visible through
`hardwareConfigDirtyError()`; a first ambiguous write error remains until newer
effect evidence exists. Configuration state becomes `UNKNOWN`, and only a
successful full replay/readback returns it to `VERIFIED`.

Cancellation and timeout may leave a bus-silent reconciliation wait active
after the original deadline. Keep calling `poll()` with advancing time until
`done`, then consume the matching terminal token. A completed quiet wait can
still leave uncertain conversion state; it cannot stop unexpectedly continuous
hardware. Explicit recovery/profile apply or shutdown then requests and
verifies idle. Shutdown alone does not verify the complete threshold profile or
clear an existing dirty diagnostic.

The synchronous `begin()`/`recover()`/verified-apply facades share that engine.
They can use a clockless idle fast path, but a required timed wait without
`Config::nowMs` returns `INVALID_CONFIG` with work still active. A stopped clock
returns `CLOCK_STALLED`; a transport failure may similarly leave reconciliation
pending. Resume the active token through advancing owner `poll()` time in its
original domain (zero at start when no clock hook was supplied), and consume
the result before scheduling new work.

## ESP-IDF Error Mapping Limits

The diagnostic example's `i2c_master` APIs return broad `esp_err_t` values and
do not prove precise address-NACK versus data-NACK classification.

| ESP-IDF result | Library mapping | Production interpretation |
| --- | --- | --- |
| `ESP_OK` | `Err::OK` | Transfer completed. |
| `ESP_ERR_TIMEOUT` | `Err::I2C_TIMEOUT` | Backend timeout; does not establish target absence or the failed bus phase. |
| `ESP_ERR_INVALID_STATE` / `ESP_ERR_INVALID_ARG` from IDF | `Err::I2C_BUS` | Adapter/backend failure; conservatively retains possible write effects. |
| Adapter rejects missing context, address, timeout, or buffers before calling IDF | `Err::INVALID_CONFIG` / `Err::INVALID_PARAM` | No transfer was attempted. |
| Proven address NACK | `Err::I2C_NACK_ADDR` | Use only when the selected IDF/backend can prove the address phase. |
| Proven data NACK | `Err::I2C_NACK_DATA` | Use only when the payload phase is distinguishable. |
| Other `esp_err_t` | `Err::I2C_ERROR`, raw code in `detail` | Conservative fallback. |

## Verification Scope

The Arduino PlatformIO stack and independently pinned native ESP-IDF CI
baseline are documented with the complete command set in the
[`README`](../README.md#validation-and-reproducibility). Local claims require
actual command output; configured CI is not evidence that an unobserved local
command passed. Native ESP-IDF target HIL and final-board electrical, fault,
cancellation, shared-bus, and workload validation remain in
[`OPEN_ITEMS.md`](OPEN_ITEMS.md).
