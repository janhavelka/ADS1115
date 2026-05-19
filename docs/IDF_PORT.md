# ADS1115 ESP-IDF Port Status

Last updated: 2026-05-19

This repository now includes the framework-neutral core changes and a native
ESP-IDF example that exposes the same user-visible CLI as the Arduino example. See
`docs/IDF_PORT_IMPLEMENTATION.md` for the implementation summary and remaining
hardware-validation blockers.

Official ESP-IDF references:
- I2C master driver: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html
- ESP-IDF v6.0 peripheral migration guide: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/migration-guides/release-6.x/6.0/peripherals.html

## Current Framework/Library State

- `library.json` version is `1.0.0`; the package declares `arduino` and
  `espidf` frameworks on `espressif32`.
- `platformio.ini` builds the Arduino CLI example from
  `examples/01_basic_bringup_cli`, `examples/common`, `src`, and `include`;
  it also has a native Unity test environment.
- Public API is under `include/ADS1115/` and is already transport-agnostic at
  the interface boundary.
- `include/ADS1115/Config.h` exposes `I2cWriteFn`, `I2cWriteReadFn`,
  optional `GpioReadFn`, `NowMsFn`, and `YieldFn`.
- `include/ADS1115/ADS1115.h` exposes the managed synchronous lifecycle
  `Status begin(const Config&)`, `void tick(uint32_t nowMs)`, `void end()`,
  raw register access, single-shot/continuous conversion helpers,
  comparator/ALERT/RDY helpers, and four-state health tracking.
- `src/ADS1115.cpp` correctly routes I2C through `_i2cWriteReadRaw`,
  `_i2cWriteRaw`, `_i2cWriteReadTracked`, and `_i2cWriteTracked`; health is
  updated from tracked wrappers.
- The library core does not include Arduino headers and does not call `Wire`,
  `Serial`, `millis()`, `delay()`, or `yield()`.
- Timing and yield ownership are explicit application callbacks via
  `Config::nowMs` and `Config::cooperativeYield`.
- Arduino-only glue lives in `examples/common/I2cTransport.h`,
  `I2cScanner.h`, `BoardConfig.h`, and the CLI example. Keep that glue out of
  the library component.

Readiness verdict: the core is IDF-ready at the source boundary and includes an
ESP-IDF example using the new I2C master driver plus the full bring-up CLI.
Remaining work is build verification on the local ESP-IDF toolchain and
hardware validation.

## Portability Blockers

- Pure IDF compilation depends on the local ESP-IDF toolchain being installed
  and sourced before running `idf.py`.
- The neutral core fallback for missing timing callbacks returns `0` and no-ops
  yield. Production applications should always provide `nowMs` and
  `cooperativeYield`.
- The IDF adapter uses the new ESP-IDF I2C master driver
  `<driver/i2c_master.h>` from component `esp_driver_i2c`.
- `examples/01_basic_bringup_cli/main.cpp` is shared by Arduino and ESP-IDF.
  Arduino builds use `Serial`, `String`, and `Wire`; the ESP-IDF example uses
  example-local console/String/timing and I2C shims instead.
- ALERT/RDY support depends on `Config::gpioRead`; an IDF example must provide a
  GPIO callback if it enables conversion-ready pin mode.
- The IDF v6.0.1 warning profile is stricter. Build the component with warnings
  enabled early and fix unused variables, implicit conversions, and signed/size
  comparisons before enabling warnings-as-errors.

## Exact Files/APIs Changed

- `src/ADS1115.cpp`
  - Removed the unconditional `#include <Arduino.h>`.
  - Keep `_i2cWriteReadRaw()`, `_i2cWriteRaw()`,
    `_i2cWriteReadTracked()`, and `_i2cWriteTracked()` as the only transport
    path.
  - `_nowMs()` and `_cooperativeYield()` now only use configured callbacks.
  - Do not add direct `i2c_master_*` calls to the core driver.
- `include/ADS1115/Config.h`
  - Preserve the existing callback signatures; they are already suitable for
    both Arduino and ESP-IDF.
  - Documents that applications should set `nowMs` and `cooperativeYield`.
  - Do not add `driver/i2c_master.h` to this public header unless an optional
    IDF adapter header is introduced separately.
- `include/ADS1115/ADS1115.h`
  - Preserve the public class name, namespace, enums, `Status` return contract,
    and health counters.
- Added a root `CMakeLists.txt` for ESP-IDF component builds.
- Added `idf_component.yml`.
- Added IDF-only adapter/example files under `examples/esp_idf/basic/`.

## Proposed Architecture Preserving Arduino Compatibility

- Keep the library core callback-based and framework-neutral.
- Keep `examples/common/I2cTransport.h` as the Arduino/Wire adapter in Arduino
  builds and as a selector for the ESP-IDF adapter when
  `ADS1115_EXAMPLE_PLATFORM_IDF` is defined.
- Keep the IDF adapter outside the core driver in
  `examples/esp_idf/basic/main/Ads1115IdfI2cTransport.{h,cpp}`.
- The IDF adapter owns `i2c_master_bus_handle_t` and
  `i2c_master_dev_handle_t`; the ADS1115 driver only receives
  `Config::i2cWrite`, `Config::i2cWriteRead`, and a user pointer.
- Keep GPIO and timing as callbacks. If ALERT/RDY is used, the IDF application
  configures the GPIO and passes `gpio_get_level()` through `Config::gpioRead`.
- Preserve the existing health model. `probe()` must remain raw/no-health;
  normal register helpers must continue using tracked wrappers.

## IDF Transport Adapter Contract

The adapter should use the ESP-IDF v6.0.1 new I2C master driver only:

```cpp
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

struct Ads1115IdfI2c {
  i2c_master_bus_handle_t bus = nullptr;
  i2c_master_dev_handle_t dev = nullptr;
  uint8_t address = 0x48;
};
```

Callback behavior:
- `i2cWrite(addr, data, len, timeoutMs, user)` calls
  `i2c_master_transmit(dev, data, len, timeoutMs)`.
- `i2cWriteRead(addr, txData, txLen, rxData, rxLen, timeoutMs, user)` calls
  `i2c_master_transmit_receive(dev, txData, txLen, rxData, rxLen, timeoutMs)`.
- The adapter must be synchronous from the ADS1115 driver's point of view.
  Do not register `i2c_master_register_event_callbacks()` on this device
  handle unless the adapter waits for completion before returning to the
  driver; health counters are updated immediately after the callback returns.
- Clamp or reject `timeoutMs` before passing it to ESP-IDF's signed
  `xfer_timeout_ms` parameter. Never let a large `uint32_t` become `-1`,
  because `-1` means wait forever in the IDF I2C API.
- The callback should reject an address that does not match the configured
  device handle and return `Err::INVALID_CONFIG` or `Err::I2C_BUS`.
- Map `ESP_OK` to `Status::Ok()`.
- Map `ESP_ERR_TIMEOUT` to `Err::I2C_TIMEOUT`.
- Map `ESP_ERR_INVALID_RESPONSE` to an I2C NACK-related status. The simple
  ESP-IDF master APIs do not distinguish address and data phase, so prefer
  `Err::I2C_ERROR` with `Status.detail = ESP_ERR_INVALID_RESPONSE` unless a
  custom adapter can prove the phase.
- Map other transport failures to `Err::I2C_BUS` or `Err::I2C_ERROR`, preserving
  the raw `esp_err_t` in `Status::detail`.
- Use `Config::i2cTimeoutMs` directly as milliseconds; do not convert it to
  FreeRTOS ticks for `i2c_master_transmit*()`.
- `nowMs(user)` should return `esp_timer_get_time() / 1000`.
- `cooperativeYield(user)` should call `taskYIELD()` or `vTaskDelay(1)` based
  on the example's scheduling policy.

## Component/CMake Layout

Recommended component layout:

```text
ADS1115/
  CMakeLists.txt
  include/ADS1115/*.h
  src/ADS1115.cpp
  examples/esp_idf/basic/
    CMakeLists.txt
    main/CMakeLists.txt
    main/main.cpp
    main/Ads1115IdfI2cTransport.cpp
```

Core-only component:

```cmake
idf_component_register(
  SRCS "src/ADS1115.cpp"
  INCLUDE_DIRS "include"
  INCLUDE_DIRS "${ADS1115_GENERATED_INCLUDE_DIR}" "include"
)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
```

If the IDF I2C adapter is shipped inside the component, add its source file and
`PRIV_REQUIRES esp_driver_i2c esp_driver_gpio esp_timer esp_rom freertos log vfs`.
The adapter currently stays in the example, so those requirements live in the
example component instead.

`include/ADS1115/Version.h` is generated from `library.json` in the current
PlatformIO flow. The IDF build must either run the same generation step before
CMake configure or add a checked-in release artifact policy so IDF users do not
build against stale version metadata.

## Example Plan

- Keep the existing Arduino CLI example building with PlatformIO for ESP32-S2
  and ESP32-S3.
- `examples/esp_idf/basic/main/main.cpp` defines
  `ADS1115_EXAMPLE_PLATFORM_IDF`, includes the example-local compatibility
  layer, and then includes `examples/01_basic_bringup_cli/main.cpp`.
- The ESP-IDF CLI exposes the same help grouping, color output, channel/gain/
  rate/mode commands, comparator commands, register diagnostics, health,
  probe/recover, stress, and self-test flows as the Arduino CLI.
- `examples/common/IdfArduinoCompat.h` provides the small `Serial`, fixed-size
  `String`, timing, delay, yield, and `F()` surface needed by the shared CLI.

## Test/Validation Plan

- Static checks:
  - `python tools/check_cli_contract.py`
  - `python tools/check_idf_example_contract.py`
  - `python tools/check_core_timing_guard.py`
  - `rg "<Arduino.h>|<Wire.h>|millis\\(|delay\\(|yield\\(" include src`
    should find no unguarded Arduino dependencies in the ESP-IDF build path.
  - `rg "driver/i2c.h|i2c_cmd_link|i2c_driver_install" .` should not find
    legacy I2C driver usage in IDF code.
- Arduino regression:
  - `pio test -e native`
  - `pio run -e esp32s3dev`
  - `pio run -e esp32s2dev`
- Arduino-ESP32 builds are regression checks only. As of 2026-05-17 the
  current Arduino-ESP32 release line is based on ESP-IDF 5.5.x, so these builds
  do not prove pure ESP-IDF v6.0.1 compatibility.
- IDF build:
  - `idf.py set-target esp32s3 build` from `examples/esp_idf/basic`
  - `idf.py set-target esp32s2 build` from `examples/esp_idf/basic`
- Hardware validation:
  - `begin()` reads the CONFIG register at addresses `0x48` through `0x4B`
    when hardware straps allow it.
  - Single-shot conversion returns `Err::IN_PROGRESS`, then readiness, then a
    signed raw code.
  - Continuous mode reads at the configured data rate without blocking forever.
  - ALERT/RDY GPIO mode reports ready state correctly for active-low and
    active-high settings.
  - Forced NACK/timeout paths update `lastError()`, `consecutiveFailures()`,
    and OFFLINE transition behavior.

## ESP-IDF v6.0.1 Migration Hazards

- Do not use the legacy I2C header `<driver/i2c.h>` or command-link APIs.
  New code must use `<driver/i2c_master.h>` and declare `esp_driver_i2c`.
- `ESP_ERR_INVALID_RESPONSE` is the expected new-driver NACK signal. Preserve it
  in `Status::detail`.
- ESP-IDF components must declare split driver dependencies explicitly.
- IDF v6 treats more compiler warnings as build failures in common project
  profiles; keep the component clean under `-Wall -Wextra`.
- Do not let Arduino fallback timing hide missing IDF timing hooks. A pure IDF
  example should work with no Arduino headers in the compile graph.
- Keep I2C bus ownership outside the library. The application or IDF example
  configures pins, clock, pull-ups, and bus lifetime.

## Implementation Checklist

1. Done: add the root `CMakeLists.txt` for a core-only IDF component.
2. Done: remove Arduino include and timing/yield fallbacks from
   `src/ADS1115.cpp`.
3. Done: add the IDF I2C adapter using `<driver/i2c_master.h>`.
4. Done: add `examples/esp_idf/basic` with the shared full CLI.
5. Done: add static Arduino/IDF CLI contract checks.
6. Done: run Arduino PlatformIO builds and native tests to verify compatibility.
7. Pending local ESP-IDF toolchain: build `examples/esp_idf/basic` for ESP32-S3.
8. Pending local ESP-IDF toolchain: build `examples/esp_idf/basic` for ESP32-S2.
9. Pending hardware: validate reads, conversion timing, voltage scaling, and
   ALERT/RDY.
10. Pending hardware/test double: inject NACK and timeout failures and verify
   status/health behavior.
