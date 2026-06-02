#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

REQUIRED_COMMON = [
    "BoardConfig.h",
    "BuildConfig.h",
    "Log.h",
    "I2cTransport.h",
    "I2cScanner.h",
    "CommandHandler.h",
    "TransportAdapter.h",
    "BusDiag.h",
    "CliShell.h",
    "CliStyle.h",
    "HealthView.h",
    "HealthDiag.h",
]

MANDATORY_COMMANDS = ["help", "scan", "probe", "recover", "drv", "read", "addr", "verbose", "stress"]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def ensure_exists(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")


def ensure_missing(path: pathlib.Path, label: str) -> None:
    if path.exists():
        fail(f"forbidden {label} still present: {path.as_posix()}")


def main() -> int:
    common_dir = ROOT / "examples" / "common"
    bringup_main = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
    hil_capture = ROOT / "tools" / "hil_ads1115_capture.py"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(bringup_main, "bringup CLI example")
    ensure_exists(hil_capture, "HIL capture helper")

    ensure_missing(ROOT / "examples" / "00_smoke_boot", "deprecated example 00_smoke_boot")
    ensure_missing(
        ROOT / "examples" / "03_feature_walkthrough",
        "deprecated example 03_feature_walkthrough",
    )

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    text = bringup_main.read_text(encoding="utf-8", errors="replace")
    hil_text = hil_capture.read_text(encoding="utf-8", errors="replace")
    readme = (ROOT / "README.md").read_text(encoding="utf-8", errors="replace")

    for cmd in MANDATORY_COMMANDS:
        if re.search(rf"\b{re.escape(cmd)}\b", text) is None:
            fail(f"mandatory command '{cmd}' missing in {bringup_main.as_posix()}")

    if re.search(r"\bcfg\b", text) is None and re.search(r"\bsettings\b", text) is None:
        fail("either 'cfg' or 'settings' command must be present")

    for token in (
        "beginDriverAtAddress",
        "activeI2cAddress",
        'cmd.startsWith("addr ")',
        'cmd.startsWith("wreg ")',
        "writeRegister16",
        "hardwareConfigDirty",
        "marks cache dirty",
        "requestedI2cAddress",
        "lastAddressSelectionStatus",
        "probeAddressRaw",
        "Address note: requested",
        "Address selection failed; initialized driver was left unchanged",
    ):
        if token not in text:
            fail(f"bringup CLI must include token: {token!r}")

    for token in (
        "RESTORE_COMMANDS",
        "response_is_ready",
        "command_is_functional",
        "response_has_prompt",
        "command_timeout_s",
        "Command timed out before CLI prompt",
        "classify_address_response",
        "Selftest precondition failed for the requested address",
        "Restore failed; aborting HIL capture before functional commands.",
        "Address check",
    ):
        if token not in hil_text:
            fail(f"HIL capture helper must include token: {token!r}")

    for token in (
        "diagnostic Arduino bring-up CLI",
        "Current examples are diagnostic",
        "Production applications should implement",
        "Raw writes bypass the typed config helpers",
    ):
        if token not in readme:
            fail(f"README must document example honesty token: {token!r}")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
