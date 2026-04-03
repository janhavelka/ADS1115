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
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;  // Manager-owned in shared-bus setups.
  wire->beginTransmission(addr);
  wire->write(data, len);
  switch (wire->endTransmission(true)) {
    case 0: return ADS1115::Status::Ok();
    case 2: return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_ADDR, "Address NACK");
    case 3: return ADS1115::Status::Error(ADS1115::Err::I2C_NACK_DATA, "Data NACK");
    case 5: return ADS1115::Status::Error(ADS1115::Err::I2C_TIMEOUT, "I2C timeout");
    case 4: return ADS1115::Status::Error(ADS1115::Err::I2C_BUS, "I2C bus error");
    default: return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Write failed");
  }
}

ADS1115::Status i2cWriteRead(uint8_t addr, const uint8_t* tx, size_t txLen,
                             uint8_t* rx, size_t rxLen,
                             uint32_t timeoutMs, void* user) {
  TwoWire* wire = static_cast<TwoWire*>(user);
  (void)timeoutMs;  // Manager-owned in shared-bus setups.
  wire->beginTransmission(addr);
  wire->write(tx, txLen);
  uint8_t result = wire->endTransmission(false);
  if (result != 0) {
    return ADS1115::Status::Error(
      result == 2 ? ADS1115::Err::I2C_NACK_ADDR :
      result == 3 ? ADS1115::Err::I2C_NACK_DATA :
      result == 5 ? ADS1115::Err::I2C_TIMEOUT :
      result == 4 ? ADS1115::Err::I2C_BUS :
                    ADS1115::Err::I2C_ERROR,
      "Write phase failed");
  }
  if (wire->requestFrom(addr, rxLen) != rxLen) {
    return ADS1115::Status::Error(ADS1115::Err::I2C_ERROR, "Read failed");
  }
  for (size_t i = 0; i < rxLen; ++i) {
    rx[i] = wire->read();
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
  cfg.i2cUser = &Wire;
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

### Example Helpers (`examples/common/`)

Not part of the library. These simulate project-level glue and keep examples self-contained:

| File | Purpose |
|------|---------|
| `BoardConfig.h` | Pin definitions and Wire init for supported boards |
| `BuildConfig.h` | Compile-time `LOG_LEVEL` configuration |
| `Log.h` | Serial logging macros (`LOGE`/`LOGW`/`LOGI`/`LOGD`/`LOGT`/`LOGV`) |
| `I2cTransport.h` | Wire-based I2C transport adapter (`wireWrite`, `wireWriteRead`, `initWire`) |
| `I2cScanner.h` | I2C bus scanner with table output and bus recovery |
| `BusDiag.h` | Bus diagnostics wrapper (scan + probe) |
| `CliShell.h` | Serial command-line shell with line editing |
| `CommandHandler.h` | Command parsing helpers (`readLine`, `match`, `parseInt`) |
| `HealthView.h` | Compact health status display |
| `HealthDiag.h` | Verbose health diagnostics with color, snapshots, and `HealthMonitor` |
| `TransportAdapter.h` | Transport function pointer adapter |

## Documentation

- `CHANGELOG.md` - full release history
- `release_notes.md` - latest release summary
- `docs/IDF_PORT.md` - ESP-IDF portability guidance
- `docs/ads1115.pdf` - TI datasheet copy used for driver verification
- `docs/TI_registry_reference/README.md` - TI reference-driver extraction notes

## License

MIT License. See `LICENSE`.
