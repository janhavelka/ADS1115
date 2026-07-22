# ADS1115 ESP-IDF Port Implementation

> Historical implementation record. Use [`../../IDF_PORT.md`](../../IDF_PORT.md)
> for the current integration contract and [`../../OPEN_ITEMS.md`](../../OPEN_ITEMS.md)
> for unfinished work.

Date: 2026-05-19

## Implemented

- `include/` and `src/` no longer include Arduino headers or call Arduino timing APIs.
- I2C remains fully application-owned through `Config::i2cWrite` and
  `Config::i2cWriteRead`.
- Timing/yield behavior is application-owned through `Config::nowMs` and
  `Config::cooperativeYield`; the core fallback is neutral and does not call a
  framework.
- Added a root ESP-IDF component `CMakeLists.txt` and `idf_component.yml`.
- Added CMake generation for the public `ADS1115/Version.h` header from
  `library.json` for ESP-IDF builds.
- Added `examples/esp_idf/basic`, using `app_main`, the ESP-IDF `i2c_master`
  driver, ESP-IDF GPIO, `esp_timer`, FreeRTOS scheduling, fixed command buffers,
  and stdio console input outside the ADS1115 core.
- The ESP-IDF example implements native command handling with the same
  user-visible commands, help, diagnostics, register access, comparator
  controls, stress paths, and self-test behavior as the Arduino CLI. It does
  not include Arduino CLI sources or compatibility facades.
- Kept Arduino example behavior by wiring explicit Arduino timing callbacks in
  `examples/common/I2cTransport.h` and the CLI example config.
- Added `tools/check_idf_example_contract.py` to guard native IDF dependencies,
  ban Arduino compatibility facades, and verify Arduino/IDF CLI command parity.

## Build Notes

- The ADS1115 component itself has no direct ESP-IDF peripheral dependency.
- The ESP-IDF example depends on `esp_driver_i2c`, `esp_driver_gpio`,
  `esp_timer`, and `freertos`.
- The IDF I2C adapter rejects zero or oversized timeouts before calling
  `i2c_master_transmit*()` so it never converts a driver timeout to IDF's
  wait-forever value.

## Remaining Blockers

- Hardware validation is still required for real ADS1115 devices at addresses
  `0x48` through `0x4B`.
- ALERT/RDY GPIO mode is wired through the native CLI and IDF GPIO callback, but
  still needs hardware validation for both active-low and active-high modes.
- Fault-injection validation for ESP-IDF NACK/timeout paths still needs hardware
  or an IDF-level I2C test double.
