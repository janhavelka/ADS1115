# ADS1115 ESP-IDF Portability Status

Applies to ADS1115 v2.0.

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
bounded edge/timestamp only; I2C stays in the owner task.

## Owner Integration

Use the framework-neutral owner lifecycle and deadline sizing in the
[`README`](../README.md#owner-safe-quick-start). An ESP-IDF adapter should
range-check and pass the callback timeout to the IDF transfer, serialize the
complete write/repeated-start/read sequence, and preserve the raw `esp_err_t`
in `Status::detail`. The driver already clamps the callback cap to the remaining
whole-operation time.

## Verification And Identity Limits

Owner initialization/recovery performs a CONFIG reachability read, three
writes, and three readbacks. Dynamic CONFIG OS/status bits are masked. ADS1115
has no ID register, so success is address/register plausibility, not identity.
Use a fixed board address inventory and final-board HIL.

Multi-register operations can partially reach hardware. The original transport
or readback failure remains visible through `hardwareConfigDirtyError()` and
configuration state becomes `UNKNOWN`. Only successful full replay/readback
returns it to `VERIFIED`.

## ESP-IDF Error Mapping Limits

The diagnostic example's `i2c_master` APIs return broad `esp_err_t` values and
do not prove precise address-NACK versus data-NACK classification.

| ESP-IDF result | Library mapping | Production interpretation |
| --- | --- | --- |
| `ESP_OK` | `Err::OK` | Transfer completed. |
| `ESP_ERR_TIMEOUT` | `Err::I2C_TIMEOUT` | May represent clock stretch, absent target, arbitration, or held bus; retain bus evidence. |
| `ESP_ERR_INVALID_STATE` / `ESP_ERR_INVALID_ARG` | `Err::I2C_BUS` or pre-I2C validation | Usually adapter/configuration failure, not a proven ADS1115 defect. |
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
