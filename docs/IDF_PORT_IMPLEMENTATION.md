# ADS1115 ESP-IDF Port Implementation

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
- Added `examples/esp_idf/basic`, using the ESP-IDF `i2c_master` driver,
  ESP-IDF GPIO, FreeRTOS scheduling, and stdio console glue outside the ADS1115
  core.
- The ESP-IDF example now includes the same `examples/01_basic_bringup_cli`
  source as the Arduino build through an example-local compatibility layer, so
  the serial CLI has the same commands, help, diagnostics, register access,
  comparator controls, stress paths, and self-test behavior.
- Kept Arduino example behavior by wiring explicit Arduino timing callbacks in
  `examples/common/I2cTransport.h` and the CLI example config.
- Added `tools/check_idf_example_contract.py` to guard the IDF wrapper,
  dependencies, native transport, and shared CLI command surface.

## Build Notes

- The ADS1115 component itself has no direct ESP-IDF peripheral dependency.
- The ESP-IDF example depends on `esp_driver_i2c`, `esp_driver_gpio`,
  `esp_timer`, `esp_rom`, `freertos`, `log`, and `vfs`.
- The IDF I2C adapter rejects zero or oversized timeouts before calling
  `i2c_master_transmit*()` so it never converts a driver timeout to IDF's
  wait-forever value.

## Remaining Blockers

- Hardware validation is still required for real ADS1115 devices at addresses
  `0x48` through `0x4B`.
- ALERT/RDY GPIO mode is wired through the shared CLI and IDF GPIO callback, but
  still needs hardware validation for both active-low and active-high modes.
- Fault-injection validation for ESP-IDF NACK/timeout paths still needs hardware
  or an IDF-level I2C test double.
