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
- Added `examples/esp_idf/basic`, using the ESP-IDF `i2c_master` driver outside
  the ADS1115 core.
- Kept Arduino example behavior by wiring explicit Arduino timing callbacks in
  `examples/common/I2cTransport.h` and the CLI example config.

## Build Notes

- The ADS1115 component itself has no direct ESP-IDF peripheral dependency.
- The ESP-IDF example depends on `esp_driver_i2c`, `esp_timer`, `freertos`, and
  `log`.
- The IDF I2C adapter rejects zero or oversized timeouts before calling
  `i2c_master_transmit*()` so it never converts a driver timeout to IDF's
  wait-forever value.

## Remaining Blockers

- Hardware validation is still required for real ADS1115 devices at addresses
  `0x48` through `0x4B`.
- ALERT/RDY GPIO mode is supported by the core callback contract, but the first
  IDF example covers polling only. Add a second example when hardware is
  available.
- Fault-injection validation for ESP-IDF NACK/timeout paths still needs hardware
  or an IDF-level I2C test double.
