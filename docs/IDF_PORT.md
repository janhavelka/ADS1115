# ADS1115 -- ESP-IDF Migration Prompt

> **Library**: ADS1115 (Texas Instruments 16-bit ADC)
> **Current version**: 0.1.1 -> **Target**: 2.0.0
> **Namespace**: `ADS1115`
> **Include path**: `#include "ADS1115/ADS1115.h"`
> **Difficulty**: Easy -- `millis()`/`yield()` replacement in .cpp only, I2C already callback-based

---

## Pre-Migration

```bash
git tag v0.1.1   # freeze Arduino-era version
```

---

## Current State -- Arduino Dependencies (exact)

| API | Count | Locations |
|-----|-------|-----------|
| `#include <Arduino.h>` | 1 | `.cpp` only (not in header) |
| `millis()` | 10 | Throughout .cpp |
| `yield()` | 1 | In polling loop |

I2C already abstracted via injected callbacks. Additionally uses:

```cpp
using GpioReadFn = bool (*)(void* user);  // for ALERT/RDY pin
```

Config is struct-based, time via `tick(uint32_t nowMs)`.

---

## Steps

### 1. Remove `#include <Arduino.h>`

### 2. Add timing helper at file scope in .cpp

```cpp
#include "esp_timer.h"

static inline uint32_t nowMs() {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
```

### 3. Replace all 10x `millis()` -> `nowMs()`

### 4. Replace 1x `yield()` -> `taskYIELD()`

```cpp
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// yield() -> taskYIELD();
```

### 5. Add `CMakeLists.txt` (library root)

```cmake
idf_component_register(
    SRCS "src/ADS1115.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer
)
```

### 6. Add `idf_component.yml` (library root)

```yaml
version: "2.0.0"
description: "ADS1115 16-bit ADC driver with ALERT/RDY support"
targets:
  - esp32s2
  - esp32s3
dependencies:
  idf: ">=5.0"
```

### 7. Version bump

- `library.json` -> `2.0.0`
- `Version.h` (if present) -> `2.0.0`

### 8. Replace Arduino example with ESP-IDF example

Create `examples/espidf_basic/main/main.cpp`:

```cpp
#include <cstdio>
#include "ADS1115/ADS1115.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t dev;

static ADS1115::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len, void*) {
    esp_err_t err = i2c_master_transmit(dev, data, len, 100);
    return err == ESP_OK ? ADS1115::Status{ADS1115::Err::Ok}
                         : ADS1115::Status{ADS1115::Err::I2cNack, "transmit failed"};
}

static ADS1115::Status i2cWriteRead(uint8_t addr,
                                     const uint8_t* wdata, size_t wlen,
                                     uint8_t* rdata, size_t rlen, void*) {
    esp_err_t err = i2c_master_transmit_receive(dev, wdata, wlen, rdata, rlen, 100);
    return err == ESP_OK ? ADS1115::Status{ADS1115::Err::Ok}
                         : ADS1115::Status{ADS1115::Err::I2cNack, "xfer failed"};
}

static constexpr gpio_num_t RDY_PIN = GPIO_NUM_4;

static bool readRdy(void*) {
    return gpio_get_level(RDY_PIN) != 0;
}

extern "C" void app_main() {
    // ALERT/RDY pin
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << RDY_PIN;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);

    // I2C bus
    i2c_master_bus_config_t busCfg{};
    busCfg.i2c_port = I2C_NUM_0;
    busCfg.sda_io_num = GPIO_NUM_8;
    busCfg.scl_io_num = GPIO_NUM_9;
    busCfg.clk_source = I2C_CLK_SRC_DEFAULT;
    busCfg.glitch_ignore_cnt = 7;
    busCfg.flags.enable_internal_pullup = true;
    i2c_new_master_bus(&busCfg, &bus);

    i2c_device_config_t devCfg{};
    devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    devCfg.device_address = 0x48;
    devCfg.scl_speed_hz = 400000;
    i2c_master_bus_add_device(bus, &devCfg, &dev);

    ADS1115::Config cfg{};
    cfg.i2cAddr = 0x48;
    cfg.i2cWrite = i2cWrite;
    cfg.i2cWriteRead = i2cWriteRead;
    cfg.gpioRead = readRdy;

    ADS1115::Adc adc;
    auto st = adc.begin(cfg);
    if (st.err != ADS1115::Err::Ok) {
        printf("begin() failed: %s\n", st.msg);
    }

    while (true) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        adc.tick(now);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

Create `examples/espidf_basic/main/CMakeLists.txt`:

```cmake
idf_component_register(SRCS "main.cpp" INCLUDE_DIRS "." REQUIRES driver esp_timer)
```

Create `examples/espidf_basic/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ads1115_espidf_basic)
```

---

## Verification

```bash
cd examples/espidf_basic && idf.py set-target esp32s2 && idf.py build
```

- [ ] `idf.py build` succeeds
- [ ] Zero `#include <Arduino.h>` anywhere
- [ ] Zero `millis()`, `yield()` calls remaining
- [ ] Version bumped to 2.0.0
- [ ] `git tag v2.0.0`
