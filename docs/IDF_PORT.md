# ADS1115 ESP-IDF Portability Status

Last audited: 2026-05-29

## Current Reality
- Primary checked runtime remains PlatformIO + Arduino plus native host tests.
- Core I2C access is callback-based (`Config.i2cWrite`, `Config.i2cWriteRead`).
- Timing hooks are supplied by the application (`Config.nowMs`,
  `Config.cooperativeYield`, `Config.timeUser`).
- Core logic does not include Arduino or ESP-IDF headers and does not call
  framework timing APIs directly.
- `readBlocking*()` requires `Config.nowMs`. `begin()` and nonblocking/tick
  workflows may still be used without `nowMs`, but blocking reads return
  `INVALID_CONFIG` before starting conversion when no clock hook is configured.
- A minimal pure ESP-IDF example exists at `examples/esp_idf/basic`. It is a
  build/integration example with external bus context, mutex locking, timeout
  mapping, and periodic `tick()` scheduling. Hardware behavior still requires
  board validation.

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

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example builds pass (`pio run -e esp32s3dev`, `pio run -e esp32s2dev`).
- No direct `millis/micros/yield/delay` calls or Arduino includes exist in
  `include/` or `src/`.
- Pure ESP-IDF builds should pass for `esp32s3` and `esp32s2`.
