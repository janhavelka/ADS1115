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

## Minimal Owner Pattern

```cpp
ADS1115::DriverConfig transport{};
transport.i2cWrite = idfI2cWrite;
transport.i2cWriteRead = idfI2cWriteRead;
transport.i2cUser = &applicationBusContext;
transport.transferTimeoutMs = 20;

ADS1115::DeviceProfile profile{};
profile.i2cAddress = 0x48;
profile.defaultMux = ADS1115::Mux::AIN0_GND;
profile.defaultGain = ADS1115::Gain::FSR_2_048V;
profile.dataRate = ADS1115::DataRate::SPS_128;
profile.mode = ADS1115::Mode::SINGLE_SHOT;
profile.comparator.use = ADS1115::ComparatorUse::OFF;

ADS1115::ADS1115 adc;
ADS1115::OperationToken token;
ADS1115::Status status = adc.bind(transport, profile); // zero I2C

const uint32_t nowMs = static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
if (status.ok()) {
  status = adc.startInitialize(nowMs, nowMs + 200U, token); // zero I2C
}

// Serialized I2C owner loop: at most one callback this pass.
ADS1115::PollResult progress = adc.poll(ownerNowMs, 1);
if (progress.done) {
  ADS1115::OperationResult result;
  status = adc.takeResult(token, result); // zero I2C
}
```

The adapter should range-check and pass the callback timeout to the IDF transfer,
serialize the complete write/repeated-start/read sequence, and preserve the raw
`esp_err_t` in `Status::detail`. The driver already clamps the callback cap to
the remaining whole-operation time.

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

## Reproducible Verification

The Arduino PlatformIO examples pin pioarduino `55.03.311`, whose Arduino-ESP32
`3.3.11` package bundles ESP-IDF `5.5.5`. That framework stack is independent
of the native ESP-IDF example. Native CI continues to pin ESP-IDF `v5.3.5` by
container digest and builds both targets:

```bash
idf.py -C examples/esp_idf/basic set-target esp32s3 build
idf.py -C examples/esp_idf/basic set-target esp32s2 build
```

Also run:

```bash
python tools/check_core_timing_guard.py
python tools/check_idf_example_contract.py
python scripts/generate_version.py check
python -m platformio test -e native
```

Local claims require actual command output; configured CI is not evidence that
an unobserved local command passed. Native ESP-IDF target HIL and final-board
electrical, fault, cancellation, shared-bus, and workload validation remain in
[`OPEN_ITEMS.md`](OPEN_ITEMS.md).
