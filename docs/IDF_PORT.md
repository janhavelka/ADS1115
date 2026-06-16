# ADS1115 ESP-IDF Portability Status

Last audited: 2026-06-16

## Current Reality
- Primary runtime remains PlatformIO + Arduino.
- Core I2C access is callback-based (`Config.i2cWrite`, `Config.i2cWriteRead`).
- `Config.nowMs` is required; `begin()` rejects configs without it.
- `Config.cooperativeYield` remains optional and is used only by blocking convenience wrappers.
- Core driver code does not include `Arduino.h` or call Arduino timing directly.

## ESP-IDF Adapter Requirements
To run under pure ESP-IDF, provide:
1. I2C write callback.
2. I2C write-read callback.
3. Optional ALERT/RDY GPIO callback (`Config.gpioRead`) if ready pin is used.
4. Required timing callback:
   - `nowMs(user)`
5. Optional cooperative scheduler hint:
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
- Set `Config.nowMs` even when the scheduler also passes `nowMs` into `tick()`, because blocking convenience wrappers and health timestamps need the same application-owned clock.
- Transport callbacks should map native errors to `ADS1115::Status` consistently.
- Preserve timeout behavior by honoring the `timeoutMs` callback argument.
- Treat `readBlocking()` and `readBlockingVoltage()` as bounded blocking convenience APIs only. Use split start/poll/read or staged polling APIs for steady paths.
- `begin()`, `recover()`, config setters, comparator setters, `setThresholds()`, `getThresholds()`, and raw register access may perform multiple I2C transfers. Keep exact staged sequencing aligned with the companion poll-chunking work.
- Dirty/cache uncertainty after partial config apply failure is exposed through `SettingsSnapshot::hardwareConfigDirty`, `SettingsSnapshot::hardwareConfigUncertain`, `SettingsSnapshot::lastConfigApplyError`, and matching accessors.

## Verification Checklist
- `python tools/check_core_timing_guard.py` passes.
- Native tests pass (`pio test -e native`).
- Example builds pass (`pio run -e esp32s3dev`, `pio run -e esp32s2dev`).
- No direct `millis/micros/yield/delay` calls are added to core `src/` or `include/`.
