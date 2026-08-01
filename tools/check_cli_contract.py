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
    "CliStyle.h",
    "HealthView.h",
]

MANDATORY_COMMANDS = [
    "help",
    "scan",
    "probe",
    "recover",
    "shutdown",
    "drv",
    "read",
    "addr",
    "verbose",
    "stress",
    "job",
]


def fail(msg: str) -> None:
    print(f"CLI contract FAILED: {msg}")
    raise SystemExit(1)


def ensure_exists(path: pathlib.Path, label: str) -> None:
    if not path.exists():
        fail(f"missing {label}: {path.as_posix()}")


def main() -> int:
    common_dir = ROOT / "examples" / "common"
    bringup_main = ROOT / "examples" / "01_basic_bringup_cli" / "main.cpp"
    hil_runner = ROOT / "tools" / "run_i2c_hil.py"

    ensure_exists(common_dir, "common example directory")
    ensure_exists(bringup_main, "bringup CLI example")
    ensure_exists(hil_runner, "classified HIL runner")

    for name in REQUIRED_COMMON:
        ensure_exists(common_dir / name, f"common helper {name}")

    text = bringup_main.read_text(encoding="utf-8", errors="replace")
    hil_runner_text = hil_runner.read_text(encoding="utf-8", errors="replace")
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
        "applyCachedProfileVerified",
        "mutateAndVerify",
        "device.setMux",
        "printAndAcknowledgePollResult",
        "device.takeResult(result.token",
        "device.isInitialized() && !device.jobActive()",
        "DIAGNOSTIC_JOB_TIMEOUT_MS",
        "startDiagnosticSingleShotJob",
        "startDiagnosticApplyJob",
    ):
        if token not in text:
            fail(f"bringup CLI must include token: {token!r}")

    for token in (
        "TGT-{address}-MODE-RAW",
        "TGT-{address}-MODE-VOLTAGE-REJECT",
        "Read latest raw code in continuous mode",
        "Reject scaled read in continuous mode",
        '"raw", "Continuous latest-raw read"',
    ):
        if token not in hil_runner_text:
            fail(f"classified HIL runner must include token: {token!r}")
    if '"voltage", "Read voltage in continuous mode"' in hil_runner_text:
        fail("classified HIL runner must not expect scaled continuous-read success")

    for token in (
        "diagnostic Arduino bring-up CLI",
        "it is not a production bus manager",
        "does not include a shared-bus mutex",
        "Raw register writes",
        "mark cache/hardware state dirty",
    ):
        if token not in readme:
            fail(f"README must document example honesty token: {token!r}")

    print("CLI contract PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
