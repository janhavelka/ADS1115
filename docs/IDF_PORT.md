# ADS1115 ESP-IDF Portability Status

Last audited: 2026-06-01

## Current Reality
- Primary checked runtime remains PlatformIO + Arduino plus native host tests.
- Core I2C access is callback-based (`Config.i2cWrite`, `Config.i2cWriteRead`).
- Timing hooks are supplied by the application (`Config.nowMs`,
  `Config.cooperativeYield`, `Config.timeUser`).
- Core logic does not include Arduino or ESP-IDF headers and does not call
  framework timing APIs directly.
- `readBlocking*()` requires `Config.nowMs`. `begin()` and `tick(nowMs)` /
  `service(nowMs)` workflows may still be used without `Config.nowMs`; direct
  timing-based readiness checks need either `Config.nowMs`, externally supplied
  service time, or ALERT/RDY GPIO. Blocking reads return `INVALID_CONFIG` before
  starting conversion when no clock hook is configured.
- A pure ESP-IDF example exists at `examples/esp_idf/basic`. The merged example
  uses a native stdio CLI with command coverage matching the Arduino diagnostic
  CLI, split IDF transport files, external bus context, mutex locking, timeout
  propagation, coarse ESP-IDF error mapping, and periodic `tick()` scheduling.
  Hardware behavior still requires board validation.

## ESP-IDF Adapter Requirements
To run under pure ESP-IDF, provide:
1. I2C write callback.
2. I2C write-read callback.
3. Optional ALERT/RDY GPIO callback (`Config.gpioRead`) if ready pin is used.
4. `nowMs(user)` when using `readBlocking*()`.
5. Optional `cooperativeYield(user)` for scheduler-friendly blocking waits.
6. External serialization/locking around callbacks if the I2C bus is shared.

## Minimal Adapter Pattern
```cpp
static uint32_t idfNowMs(void*) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static void idfYield(void*) {
  taskYIELD();
}

ADS1115::Config cfg{};
cfg.i2cWrite = myI2cWrite;
cfg.i2cWriteRead = myI2cWriteRead;
cfg.nowMs = idfNowMs;
cfg.cooperativeYield = idfYield;
```

## Porting Notes
- Keep using `tick(nowMs)` from the application scheduler/task loop.
- Transport callbacks should map native errors to `ADS1115::Status` consistently.
- Preserve timeout behavior by honoring the `timeoutMs` callback argument.
- Do not call public driver APIs from ISR context.
- ADS1115 has no ID register; `strictInitVerify` is a read-back plausibility
  check only and masks dynamic CONFIG OS/status bits.
- Multi-register write failures can set `hardwareConfigDirty()`. A successful
  full `recover()` clears dirty state.
- The example enables ESP-IDF internal pull-ups as a convenience. Production
  boards should size external SDA/SCL pull-ups for bus capacitance, speed,
  voltage domain, and sink-current limits.

## ESP-IDF Error Mapping Limits

The example uses ESP-IDF master I2C APIs that return broad `esp_err_t` values.
It does not prove precise address-NACK versus data-NACK classification. Keep
production fault mapping conservative unless the selected ESP-IDF version,
target, and bus instrumentation prove a finer distinction.

| ESP-IDF error/fault | Current mapping | Precision limitation | Production recommendation |
| --- | --- | --- | --- |
| `ESP_OK` | `Err::OK` | None for transaction success. | Keep as success. |
| `ESP_ERR_TIMEOUT` or mutex take timeout | `Err::I2C_TIMEOUT` | Cannot by itself distinguish clock stretch, missing device, arbitration, or a held bus. | Log ESP-IDF error, bus state, target address, and recovery action; add bus-level diagnostics where needed. |
| `ESP_ERR_INVALID_STATE`, `ESP_ERR_INVALID_ARG` | `Err::I2C_BUS` or validation error before I2C | Usually adapter/configuration or bus state, not a proven ADS1115 fault. | Treat as integration/configuration failure; fix adapter setup before hardware conclusions. |
| Address NACK / missing target | Usually `Err::I2C_ERROR` or `Err::I2C_TIMEOUT` from the example | The example does not prove reliable `I2C_NACK_ADDR` classification. | Validate with scope/logic analyzer or target-specific IDF diagnostics before mapping to `I2C_NACK_ADDR`. |
| Data NACK during payload | Usually `Err::I2C_ERROR` from the example | The example does not prove reliable `I2C_NACK_DATA` classification. | Only map to `I2C_NACK_DATA` if the adapter can prove payload-phase NACK. |
| Other `esp_err_t` values | `Err::I2C_ERROR` with raw `esp_err_t` in `detail` | Coarse fallback. | Preserve `detail`, log `esp_err_to_name()`, and refine only with evidence. |

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example builds pass (`pio run -e esp32s3dev`, `pio run -e esp32s2dev`).
- No direct `millis/micros/yield/delay` calls or Arduino includes exist in
  `include/` or `src/`.
- Pure ESP-IDF coverage requires running:
  `idf.py -C examples/esp_idf/basic set-target esp32s3 build` and
  `idf.py -C examples/esp_idf/basic set-target esp32s2 build`, or citing CI logs
  that ran those exact targets.
