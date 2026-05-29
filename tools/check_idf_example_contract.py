#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_FILES = [
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/esp_idf/basic/CMakeLists.txt",
    "examples/esp_idf/basic/main/CMakeLists.txt",
    "examples/esp_idf/basic/main/main.cpp",
]

REQUIRED_TOKENS = [
    'extern "C" void app_main(void)',
    '#include "driver/i2c_master.h"',
    "esp_timer_get_time",
    "xSemaphoreTake",
    "i2c_master_transmit",
    "i2c_master_transmit_receive",
    "adc.tick",
    "vTaskDelay",
]

FORBIDDEN_TOKENS = [
    "Arduino.h",
    "Wire.h",
    "TwoWire",
    "String",
    "Serial",
    "ArduinoCompat",
    "IdfArduinoCompat",
]


def fail(message: str) -> int:
    print(f"IDF example contract FAILED: {message}")
    return 1


def main() -> int:
    for rel in REQUIRED_FILES:
      if not (ROOT / rel).exists():
        return fail(f"missing {rel}")

    text = (ROOT / "examples/esp_idf/basic/main/main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )

    for token in REQUIRED_TOKENS:
        if token not in text:
            return fail(f"missing token {token!r}")

    for token in FORBIDDEN_TOKENS:
        if token in text:
            return fail(f"forbidden token {token!r}")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
