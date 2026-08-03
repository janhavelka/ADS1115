#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

IDF_EXAMPLE_MACRO = "ADS1115_EXAMPLE_PLATFORM_IDF"
CLI_SOURCE_INCLUDE = '#include "examples/01_basic_bringup_cli/main.cpp"'
REQUIRED_COMPONENTS = [
    "ADS1115",
    "esp_driver_i2c",
    "esp_driver_gpio",
    "esp_timer",
    "freertos",
    "vfs",
]
REQUIRED_FILES = [
    "CMakeLists.txt",
    "idf_component.yml",
    "examples/esp_idf/basic/CMakeLists.txt",
    "examples/esp_idf/basic/main/CMakeLists.txt",
    "examples/esp_idf/basic/main/main.cpp",
    "examples/esp_idf/basic/main/Ads1115IdfI2cTransport.h",
    "examples/esp_idf/basic/main/Ads1115IdfI2cTransport.cpp",
]
FORBIDDEN_IDF_TOKENS = [
    "Arduino.h",
    "Wire.h",
    "TwoWire",
    "String",
    "Serial",
    "ArduinoCompat",
    "IdfArduinoCompat",
    CLI_SOURCE_INCLUDE,
]
MANDATORY_COMMANDS = [
    "help",
    "version",
    "scan",
    "addr",
    "read",
    "readv",
    "start",
    "poll",
    "raw",
    "voltage",
    "timing",
    "job",
    "ch",
    "diff",
    "gain",
    "rate",
    "mode",
    "comp",
    "config",
    "reg",
    "wreg",
    "drv",
    "state",
    "probe",
    "recover",
    "shutdown",
    "cfg",
    "verbose",
    "stress",
    "stress_mix",
    "selftest",
]
REQUIRED_IDF_TOKENS = [
    "activeI2cAddress",
    "requestedI2cAddress",
    "lastAddressSelectionStatus",
    "isValidAds1115Address",
    "makeDriverConfig",
    "probeAddressRaw",
    "beginDriverAtAddress",
    "Timebase available",
    "Hardware/cache dirty",
    "hardwareConfigDirtyAddress",
    "device.service(nowMs())",
    "device.isInitialized() && !device.jobActive()",
    "DIAGNOSTIC_JOB_TIMEOUT_MS",
    "startDiagnosticSingleShotJob",
    "startDiagnosticApplyJob",
]
def fail(msg: str) -> None:
    print(f"IDF example contract FAILED: {msg}")
    raise SystemExit(1)


def require_token(text: str, token: str, label: str) -> None:
    if token not in text:
        fail(f"{label} missing token '{token}'")


def command_has_dispatch(cli: str, command: str) -> bool:
    patterns = [
        rf'cmd\s*==\s*"{re.escape(command)}"',
        rf'cmd\.startsWith\("{re.escape(command)}\s',
        rf'cmd\.startsWith\("{re.escape(command)}"\)',
        rf'std::strcmp\(cmd,\s*"{re.escape(command)}"\)\s*==\s*0',
        rf'argAfter\(cmd,\s*"{re.escape(command)}\s',
        rf'startsWith\(cmd,\s*"{re.escape(command)}"\)',
    ]
    return any(re.search(pattern, cli) for pattern in patterns)


def main() -> int:
    for rel in REQUIRED_FILES:
        if not (ROOT / rel).exists():
            fail(f"missing {rel}")

    idf_main = (ROOT / "examples" / "esp_idf" / "basic" / "main" / "main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    for token in (
        'extern "C" void app_main(void)',
        '#include "driver/i2c_master.h"',
        "esp_timer_get_time",
        "vTaskDelay",
        "std::fgets",
        "char line[MAX_LINE_LEN]",
        "i2c_master_probe",
        "gpio_config_t",
    ):
        require_token(idf_main, token, "ESP-IDF main")

    if IDF_EXAMPLE_MACRO in idf_main:
        fail("ESP-IDF main must not enable the old shared Arduino CLI path")
    for token in FORBIDDEN_IDF_TOKENS:
        if token in idf_main:
            fail(f"ESP-IDF main contains forbidden Arduino/facade token '{token}'")
    for token in REQUIRED_IDF_TOKENS:
        require_token(idf_main, token, "ESP-IDF main")
    for token in (
        "applyCachedProfileVerified",
        "mutateAndVerify",
        "device.setMux",
        "printAndAcknowledgePollResult",
        "device.takeResult(result.token",
    ):
        require_token(idf_main, token, "ESP-IDF trust/job contract")
    if "device.tick(" in idf_main:
        fail("ESP-IDF main must use device.service(nowMs()) instead of discarding tick() status")
    if "ADS1115_I2C_ADDR" in idf_main:
        fail("ESP-IDF main must not use a fixed ADS1115_I2C_ADDR token")

    cmake = (
        ROOT / "examples" / "esp_idf" / "basic" / "main" / "CMakeLists.txt"
    ).read_text(encoding="utf-8", errors="replace")
    for component in REQUIRED_COMPONENTS:
        if re.search(rf"\b{re.escape(component)}\b", cmake) is None:
            fail(f"ESP-IDF CMake missing required component '{component}'")

    if (ROOT / "examples" / "common" / "IdfArduinoCompat.h").exists():
        fail("examples/common/IdfArduinoCompat.h must not exist")

    transport = (
        (ROOT / "examples" / "esp_idf" / "basic" / "main" / "Ads1115IdfI2cTransport.cpp")
        .read_text(encoding="utf-8", errors="replace")
        + (ROOT / "examples" / "esp_idf" / "basic" / "main" / "Ads1115IdfI2cTransport.h")
        .read_text(encoding="utf-8", errors="replace")
    )
    for token in ("driver/i2c_master.h", "i2c_new_master_bus", "i2c_master_transmit", "i2c_master_transmit_receive"):
        require_token(transport, token, "ESP-IDF transport")
    for token in FORBIDDEN_IDF_TOKENS:
        if token in transport:
            fail(f"ESP-IDF transport contains forbidden Arduino/facade token '{token}'")

    readme = (ROOT / "README.md").read_text(encoding="utf-8", errors="replace")
    idf_docs = (ROOT / "docs" / "IDF_PORT.md").read_text(encoding="utf-8", errors="replace")
    for label, text in (("README", readme), ("IDF_PORT", idf_docs)):
        if "mutex locking" in text or "locks I2C transactions with a mutex" in text:
            fail(f"{label} claims ESP-IDF mutex locking that the example does not implement")
        require_token(text, "does not include a shared-bus mutex", label)

    cli = (ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp").read_text(
        encoding="utf-8", errors="replace"
    )
    for command in MANDATORY_COMMANDS:
        if f'printHelpItem("{command}' not in cli:
            fail(f"Arduino CLI missing help item '{command}'")
        if not command_has_dispatch(cli, command):
            fail(f"Arduino CLI missing dispatch '{command}'")
        if f'printHelpItem("{command}' not in idf_main:
            fail(f"native ESP-IDF CLI missing help item '{command}'")
        if not command_has_dispatch(idf_main, command):
            fail(f"native ESP-IDF CLI missing dispatch '{command}'")

    manifest = (ROOT / "idf_component.yml").read_text(encoding="utf-8", errors="replace")
    for token in ("esp32s2", "esp32s3", "idf:"):
        require_token(manifest, token, "idf_component.yml")

    print("IDF example contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
