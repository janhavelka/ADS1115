# ADS1115 ESP-IDF Portability Status

Last audited: 2026-03-01

## Current Reality
- Primary runtime remains PlatformIO + Arduino.
- Core I2C access is callback-based (`Config.i2cWrite`, `Config.i2cWriteRead`).
- Optional timing hooks are already available (`Config.nowMs`, `Config.cooperativeYield`, `Config.timeUser`).
- Core logic does not call Arduino timing APIs directly.
- Arduino timing is used only in fallback wrappers:
  - `ADS1115::_nowMs()` -> `millis()` when `Config.nowMs == nullptr`
  - `ADS1115::_cooperativeYield()` -> `yield()` when `Config.cooperativeYield == nullptr`

## ESP-IDF Adapter Requirements
To run under pure ESP-IDF, provide:
1. I2C write callback.
2. I2C write-read callback.
3. Optional ALERT/RDY GPIO callback (`Config.gpioRead`) if ready pin is used.
4. Optional timing callbacks for full Arduino independence:
   - `nowMs(user)`
   - `cooperativeYield(user)`

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

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example builds pass (`pio run -e esp32s3dev`, `pio run -e esp32s2dev`).
- No new direct `millis/micros/yield/delay` calls are added outside portability wrappers.
