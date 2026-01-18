# {DEVICE_NAME} Driver Library — Fresh Repo Template

This is a template for creating production-grade I2C driver libraries following the **Managed Synchronous Driver** pattern with **Tracked Transport Wrappers**.

## Quick Start

1. **Copy the template:**
   ```bash
   cp -r templates/ ../MyDevice/
   cd ../MyDevice/
   ```

2. **Replace placeholders:**
   - `{DEVICE}` → Your device name (e.g., `BME280`, `ADS1115`)
   - `{DEVICE_NAME}` → Full device name (e.g., `BME280 Environmental Sensor`)
   - `{NAMESPACE}` → C++ namespace (usually same as device name)
   - `{device}` → lowercase device name for keywords

3. **Rename files:**
   ```
   include/DEVICE/        → include/YourDevice/
   include/DEVICE/DEVICE.h → include/YourDevice/YourDevice.h
   src/DEVICE.cpp         → src/YourDevice.cpp
   ```

4. **Update library.json** with your details.

5. **Add device-specific code:**
   - Register definitions in `CommandTable.h`
   - Configuration options in `Config.h`
   - API methods in `{DEVICE}.h` / `{DEVICE}.cpp`

## Template Files

```
templates/
├── AGENTS_TEMPLATE.md              # Project rules (copy as AGENTS.md)
├── README.md                       # Project README template
├── CHANGELOG.md                    # Changelog template
├── LICENSE                         # MIT License template
├── library.json                    # PlatformIO library manifest
├── platformio.ini                  # Build configuration
├── include/
│   └── DEVICE/
│       ├── Status.h                # Error codes and Status struct
│       ├── Config.h                # Configuration and transport callbacks
│       ├── CommandTable.h          # Register definitions (fill in)
│       └── DEVICE.h                # Main driver class
├── src/
│   └── DEVICE.cpp                  # Driver implementation
├── examples/
│   ├── common/
│   │   ├── Log.h                   # Logging macros
│   │   ├── BoardConfig.h           # Board-specific pins
│   │   ├── I2cTransport.h          # Wire-based transport
│   │   └── I2cScanner.h            # I2C bus scanner
│   └── 01_basic_bringup_cli/
│       └── main.cpp                # Interactive CLI example
├── test/
│   ├── stubs/
│   │   ├── Arduino.h               # Arduino stub for native tests
│   │   └── Wire.h                  # Wire stub for native tests
│   └── native/
│       └── test_basic.cpp          # Unit tests
└── scripts/
    └── generate_version.py         # Version.h generator
```

## Architecture Overview

### Transport Wrapper Pattern

```
Public API (readValue, writeConfig, etc.)
    ↓
Register helpers (readRegs, writeRegs)
    ↓
TRACKED wrappers (_i2cWriteReadTracked, _i2cWriteTracked)
    ↓  ← _updateHealth() called here ONLY
RAW wrappers (_i2cWriteReadRaw, _i2cWriteRaw)
    ↓
Transport callbacks (Config::i2cWrite, i2cWriteRead)
```

### Health Tracking

- `_updateHealth()` ONLY called inside tracked wrappers
- State transitions guarded by `_initialized` flag
- `probe()` uses raw path (no health tracking)
- `recover()` tracks failures (driver is initialized)

### Driver States

| State | Description |
|-------|-------------|
| `UNINIT` | `begin()` not called or `end()` called |
| `READY` | Operational, consecutiveFailures == 0 |
| `DEGRADED` | 1+ failures < offlineThreshold |
| `OFFLINE` | failures >= offlineThreshold |

## Customization Checklist

- [ ] Replace all `{DEVICE}`, `{NAMESPACE}`, etc. placeholders
- [ ] Rename files and directories
- [ ] Fill in register definitions in `CommandTable.h`
- [ ] Add device-specific config options in `Config.h`
- [ ] Add device-specific error codes in `Status.h` (if needed)
- [ ] Implement device-specific API methods
- [ ] Update README.md with actual API documentation
- [ ] Update AGENTS.md with device-specific requirements
- [ ] Add device-specific commands to CLI example
- [ ] Write device-specific unit tests
- [ ] Update library.json metadata

## See Also

- `docs/CODEX_PROMPT_BME280_DRIVER.md` — Example prompt for BME280
- `docs/CODEX_PROMPT_ADS1115_DRIVER.md` — Example prompt for ADS1115
- `docs/AGENTS_BME280_TEMPLATE.md` — AGENTS.md for BME280
- `docs/AGENTS_ADS1115_TEMPLATE.md` — AGENTS.md for ADS1115
