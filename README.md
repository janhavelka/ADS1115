# ADS1115 Driver Library

Production-grade ADS1115 16-bit ADC I2C driver for ESP32-S2 / ESP32-S3
(Arduino framework, PlatformIO).

## Features

- Injected I2C transport (no Wire dependency in library code)
- Health monitoring with READY / DEGRADED / OFFLINE states
- Single-shot and continuous conversion modes
- Configurable mux, gain, data rate, and comparator settings
- Raw and voltage conversion helpers

## Installation

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
  ADS1115
```

### Manual

Copy `include/ADS1115/` and `src/` into your project.

## Quick Start

```cpp
#include <Wire.h>
#include "ADS1115/ADS1115.h"

// Transport callbacks
ADS1115::Status i2cWrite(uint8_t addr, const uint8_t* data, size_t len,
                         uint32_t timeoutMs, void* user) {
  (void)user;
  Wire.setTimeOut(static_cast<uint16_t>(timeoutMs));
  Wire.beginTransmission(addr);
  Wire.write(data, len);
  return Wire.endTransmission() == 0
    ? ADS1115::Status::Ok()
    : ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Write failed");
}

ADS1115::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                             uint8_t* rx, size_t rxLen,
                             uint32_t timeoutMs, void* user) {
  (void)user;
  Wire.setTimeOut(static_cast<uint16_t>(timeoutMs));
  Wire.beginTransmission(addr);
  Wire.write(tx, txLen);
  if (Wire.endTransmission(false) != 0) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Write failed");
  }
  if (Wire.requestFrom(addr, rxLen) != rxLen) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Read failed");
  }
  for (size_t i = 0; i < rxLen; ++i) {
    rx[i] = Wire.read();
  }
  return ADS1115::Status::Ok();
}

ADS1115::ADS1115 device;

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 9);

  ADS1115::Config cfg;
  cfg.i2cWrite = i2cWrite;
  cfg.i2cWriteRead = i2cWriteRead;
  cfg.i2cAddress = 0x48;

  auto status = device.begin(cfg);
  if (!status.ok()) {
    Serial.printf("Init failed: %s\n", status.msg);
    return;
  }

  Serial.println("ADS1115 initialized!");
}

void loop() {
  device.tick(millis());
}
```

## Examples

- `examples/01_basic_bringup_cli/` - interactive CLI for ADS1115 features

## License

MIT License. See `LICENSE`.
