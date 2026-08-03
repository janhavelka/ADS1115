#!/usr/bin/env python3
"""Classified serial HIL runner for the ADS1115 diagnostic CLI.

The runner drives the repository diagnostic CLI over a real serial port. It
does not flash firmware, fake a device, or prove analog accuracy by itself; it
captures bounded command results and classifies only what the CLI output makes
observable.
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import pathlib
import re
import subprocess
import sys
import time
from collections import Counter
from typing import Iterable


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "hil_logs"
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 8.0
DEFAULT_IDLE_S = 0.35
DEFAULT_BOOT_SETTLE_S = 2.0
DEFAULT_SOAK_DURATION_S = 8 * 60 * 60
EXPECTED_ARDUINO_ESP32_VERSION = "3.3.11"
EXPECTED_ESP_IDF_VERSION = "v5.5.5"
CLI_SYNC_COMMAND = "version"
CLI_CANCEL_BYTE = b"\x18"  # ASCII CAN: firmware discards its partial input buffer.
CLI_SYNC_BYTES = CLI_CANCEL_BYTE + f"{CLI_SYNC_COMMAND}\r\n".encode("ascii")
CLI_SYNC_MARKER = "=== Version Info ==="

RESULT_PASS = "PASS"
RESULT_FAIL = "FAIL"
RESULT_EVIDENCE_REQUIRED = "EVIDENCE_REQUIRED"
RESULT_UNKNOWN = "UNKNOWN"
RESULT_NOT_RUN = "NOT_RUN"
RESULT_DRY_RUN = "DRY_RUN"

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
PROMPT_RE = re.compile(r"(^|\n)> ?$")
STATUS_RE = re.compile(r"\bStatus:\s*([A-Z0-9_]+)\b")
RAW_SAMPLE_RE = re.compile(r"\bRaw:\s*-?\d+\b")
VOLTAGE_RE = re.compile(r"\bVoltage:\s*-?\d+(?:\.\d+)?\s*V\b")
TOTAL_FAILURES_RE = re.compile(r"\bTotal failures:\s*(\d+)\b")
SELFTEST_RE = re.compile(r"Selftest result:\s*pass=.*?(\d+).*?fail=.*?(\d+).*?skip=.*?(\d+)")
STRESS_ERRORS_RE = re.compile(r"\bErrors:\s*(\d+)\b")
STRESS_MIX_SUMMARY_MARKER = "=== stress_mix summary ==="
STRESS_MIX_TOTAL_RE = re.compile(
    r"^\s*Total:\s+ok=(\d+)\s+fail=(\d+)\b",
    re.MULTILINE,
)
RATE_RE = re.compile(r"\bRate:\s*([0-9]+(?:\.[0-9]+)?)\s*(samples/s|ops/s)\b")
DURATION_RE = re.compile(r"\bDuration:\s*(\d+)\s*ms\b")
FIRMWARE_COMMIT_RE = re.compile(
    r"ADS1115 library commit:\s*([0-9a-f]+)\s*\((clean|dirty)\)",
    re.IGNORECASE,
)
ARDUINO_ESP32_VERSION_RE = re.compile(r"^\s*Arduino-ESP32:\s*([^\r\n]+)", re.MULTILINE)
ESP_IDF_VERSION_RE = re.compile(r"^\s*ESP-IDF:\s*([^\r\n]+)", re.MULTILINE)
FOUND_ADDRESS_RE = re.compile(r"\b(?:0x)?4[89AB]\b", re.IGNORECASE)
SCAN_40_ROW_RE = re.compile(r"^\s*40:\s*(.*)$", re.IGNORECASE | re.MULTILINE)
BATCH_THREE_RE = re.compile(r"^\s*3:\s*-?\d+\s+\(", re.MULTILINE)
REBOOT_MARKERS = (
    "=== ADS1115 Diagnostic Bring-up CLI ===",
    "Reset reason:",
)

STATUS_FAILURES = {
    "DEVICE_NOT_FOUND",
    "CONVERSION_NOT_READY",
    "I2C_NACK_ADDR",
    "I2C_NACK_DATA",
    "I2C_TIMEOUT",
    "I2C_BUS",
    "I2C_ERROR",
    "TIMEOUT",
    "INVALID_CONFIG",
    "INVALID_PARAM",
    "NOT_INITIALIZED",
    "OFFLINE",
    "READBACK_MISMATCH",
    "UNSUPPORTED_OPERATION",
    "HARDWARE_CONFIG_DIRTY",
    "CLOCK_STALLED",
    "CANCELLED",
    "CONFIG_UNKNOWN",
    "RESULT_NOT_AVAILABLE",
    "TOKEN_MISMATCH",
    "INDETERMINATE",
}

GENERIC_FAILURE_RE = (
    re.compile(r"\bFAILED\b", re.IGNORECASE),
    re.compile(r"\[FAIL\]", re.IGNORECASE),
    re.compile(r"\[E\]"),
)


@dataclasses.dataclass(frozen=True)
class CommandSpec:
    test_id: str
    feature: str
    command: str
    expected: str
    expected_any: tuple[str, ...] = ()
    validators: tuple[str, ...] = ()
    failure_tokens: tuple[str, ...] = ()
    timeout_s: float = DEFAULT_TIMEOUT_S
    notes: str = ""
    post_delay_s: float = 0.0
    expected_failure: bool = False
    unknown_on_pass: bool = False

    def formatted(self, *, address: str) -> "CommandSpec":
        return dataclasses.replace(
            self,
            test_id=self.test_id.format(address=address.replace("0x", "")),
            command=self.command.format(address=address),
            expected=self.expected.format(address=address),
            notes=self.notes.format(address=address),
        )


@dataclasses.dataclass
class StepResult:
    test_id: str
    feature: str
    command: str
    expected: str
    observed: str
    elapsed_s: float
    result: str
    notes: str
    evidence_required: bool = False
    reset_observed: bool = False


@dataclasses.dataclass
class SoakStats:
    start: dt.datetime
    end: dt.datetime | None = None
    commands: Counter[str] = dataclasses.field(default_factory=Counter)
    results: Counter[str] = dataclasses.field(default_factory=Counter)
    latencies: list[float] = dataclasses.field(default_factory=list)
    worst_read_latency_s: float = 0.0
    consecutive_failures: int = 0
    max_consecutive_failures: int = 0
    cycles: int = 0
    stopped_reason: str = ""
    reset_markers: int = 0


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text).replace("\r", "")


def summarize_observed(text: str, limit: int = 180) -> str:
    plain = " ".join(line.strip() for line in strip_ansi(text).splitlines() if line.strip())
    if len(plain) <= limit:
        return plain
    return plain[: limit - 3] + "..."


def has_prompt(text: str) -> bool:
    return bool(PROMPT_RE.search(strip_ansi(text)))


def has_completed_cli_sync(text: str) -> bool:
    """Return true only for a version response followed by its own prompt."""
    plain = strip_ansi(text)
    marker_index = plain.rfind(CLI_SYNC_MARKER)
    return marker_index >= 0 and bool(PROMPT_RE.search(plain[marker_index:]))


def run_git(args: list[str]) -> str:
    try:
        result = subprocess.run(
            ["git", *args],
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
    except Exception as exc:
        return f"unavailable ({exc})"
    if result.returncode != 0:
        return (result.stderr or result.stdout).strip() or "unavailable"
    return result.stdout.strip() or "unknown"


def parse_address(value: str) -> str:
    try:
        parsed = int(value, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("address must be 0x48, 0x49, 0x4A, or 0x4B") from exc
    if parsed < 0x48 or parsed > 0x4B:
        raise argparse.ArgumentTypeError("address must be 0x48, 0x49, 0x4A, or 0x4B")
    return f"0x{parsed:02X}"


def status_token(text: str) -> str | None:
    match = STATUS_RE.search(strip_ansi(text))
    return match.group(1) if match else None


def output_has_failure(spec: CommandSpec, text: str) -> str | None:
    plain = strip_ansi(text)
    for marker in REBOOT_MARKERS:
        if marker in plain:
            return f"unexpected reboot marker {marker}"
    token = status_token(plain)
    if token in STATUS_FAILURES:
        return f"status token {token}"
    for failure in spec.failure_tokens:
        if failure in plain:
            return f"failure token {failure}"
    for pattern in GENERIC_FAILURE_RE:
        if pattern.search(plain):
            return f"failure pattern {pattern.pattern}"
    return None


def expected_found(spec: CommandSpec, text: str) -> bool:
    plain = strip_ansi(text)
    return not spec.expected_any or any(token in plain for token in spec.expected_any)


def parse_int(pattern: re.Pattern[str], text: str) -> int | None:
    match = pattern.search(strip_ansi(text))
    if not match:
        return None
    try:
        return int(match.group(1))
    except ValueError:
        return None


def parse_stress_mix_fail_count(text: str) -> int | None:
    plain = strip_ansi(text)
    section = plain.rsplit(STRESS_MIX_SUMMARY_MARKER, 1)[-1]
    matches = list(STRESS_MIX_TOTAL_RE.finditer(section))
    if not matches:
        return None
    try:
        return int(matches[-1].group(2))
    except ValueError:
        return None


def validate_output(spec: CommandSpec, text: str) -> str | None:
    plain = strip_ansi(text)
    for validator in spec.validators:
        if validator == "status_ok":
            if status_token(plain) != "OK":
                return "Status: OK not found"
        elif validator == "status_non_ok":
            token = status_token(plain)
            if token is None or token == "OK":
                return "non-OK status not found"
        elif validator == "status_busy":
            if status_token(plain) != "BUSY":
                return "Status: BUSY not found"
        elif validator == "status_in_progress":
            if status_token(plain) != "IN_PROGRESS":
                return "Status: IN_PROGRESS not found"
        elif validator == "status_unsupported":
            if status_token(plain) != "UNSUPPORTED_OPERATION":
                return "Status: UNSUPPORTED_OPERATION not found"
        elif validator == "status_cancelled":
            if status_token(plain) != "CANCELLED":
                return "Status: CANCELLED not found"
        elif validator == "driver_ready":
            if "State: READY" not in plain and "state=READY" not in plain:
                return "driver state is not READY"
        elif validator == "firmware_clean_commit":
            match = FIRMWARE_COMMIT_RE.search(plain)
            if match is None:
                return "firmware commit/clean marker not found"
            firmware_commit = match.group(1).lower()
            host_commit = run_git(["rev-parse", "HEAD"]).lower()
            if match.group(2).lower() != "clean":
                return "firmware reports a dirty build"
            if not host_commit.startswith(firmware_commit):
                return f"firmware commit {firmware_commit} does not match host {host_commit}"
        elif validator == "runtime_stack":
            arduino_match = ARDUINO_ESP32_VERSION_RE.search(plain)
            if arduino_match is None:
                return "Arduino-ESP32 runtime version not found"
            arduino_version = arduino_match.group(1).strip()
            if arduino_version != EXPECTED_ARDUINO_ESP32_VERSION:
                return (
                    f"Arduino-ESP32 {arduino_version} does not match "
                    f"{EXPECTED_ARDUINO_ESP32_VERSION}"
                )
            idf_match = ESP_IDF_VERSION_RE.search(plain)
            if idf_match is None:
                return "ESP-IDF runtime version not found"
            idf_version = idf_match.group(1).strip()
            if idf_version != EXPECTED_ESP_IDF_VERSION:
                return f"ESP-IDF {idf_version} does not match {EXPECTED_ESP_IDF_VERSION}"
        elif validator == "zero_failures":
            failures = parse_int(TOTAL_FAILURES_RE, plain)
            if failures is None:
                return "total failure count not found"
            if failures != 0:
                return f"total failures={failures}"
        elif validator == "raw_sample":
            if RAW_SAMPLE_RE.search(plain) is None:
                return "raw sample not found"
        elif validator == "voltage":
            if VOLTAGE_RE.search(plain) is None and "Blocking voltage:" not in plain:
                return "voltage output not found"
        elif validator == "selftest":
            match = SELFTEST_RE.search(plain)
            if not match:
                return "selftest summary not found"
            if int(match.group(2)) != 0:
                return f"selftest failures={match.group(2)}"
            if int(match.group(3)) != 0:
                return f"selftest skipped={match.group(3)}"
        elif validator == "stress_zero_errors":
            errors = parse_int(STRESS_ERRORS_RE, plain)
            if errors is None:
                return "stress error count not found"
            if errors != 0:
                return f"stress errors={errors}"
        elif validator == "stress_mix_zero_fail":
            failures = parse_stress_mix_fail_count(plain)
            if failures is None:
                return "stress_mix fail count not found"
            if failures != 0:
                return f"stress_mix fail={failures}"
        elif validator == "job_active":
            if "Active: YES" not in plain:
                return "job is not active"
        elif validator == "job_inactive":
            if "Active: NO" not in plain:
                return "job is still active"
        elif validator == "job_done":
            if "Done: YES" not in plain:
                return "job did not complete"
            if status_token(plain) != "OK":
                return "completed job status is not OK"
        elif validator == "job_zero_budget":
            if "Instructions used: 0" not in plain:
                return "zero-budget poll consumed instructions"
        elif validator == "no_active_job":
            if "No active pollable job" not in plain:
                return "no-active-job message not found"
        elif validator == "no_health_changes":
            if "(no health changes)" not in plain:
                return "probe changed health counters"
        elif validator == "dirty_yes":
            if "Hardware/cache dirty: YES" not in plain:
                return "dirty diagnostic not visible"
        elif validator == "dirty_no":
            if "Hardware/cache dirty: NO" not in plain:
                return "dirty diagnostic did not clear"
        elif validator == "scan_ads1115":
            found = {
                match.upper()
                for row in SCAN_40_ROW_RE.findall(plain)
                for match in FOUND_ADDRESS_RE.findall(row)
            }
            if not found:
                return "no ADS1115-range address found in scan"
        elif validator == "batch_three":
            if BATCH_THREE_RE.search(plain) is None:
                return "third batch sample not found"
        elif validator == "rate_report":
            if RATE_RE.search(plain) is None or DURATION_RE.search(plain) is None:
                return "rate/duration summary not found"
        elif validator == "invalid_or_usage":
            if not any(token in plain for token in ("Invalid", "Usage:", "Unknown command", "must be")):
                return "validation error text not found"
        else:
            return f"unknown validator {validator}"
    return None


def classify_output(spec: CommandSpec, text: str, *, timed_out: bool = False) -> tuple[str, str]:
    if timed_out:
        return RESULT_FAIL, "serial prompt was not observed before timeout"
    failure = output_has_failure(spec, text)
    if spec.expected_failure:
        validation = validate_output(spec, text)
        if validation is not None:
            return RESULT_FAIL, validation
        return RESULT_PASS, failure or "expected validation/error path observed"
    if failure is not None:
        return RESULT_FAIL, failure
    validation = validate_output(spec, text)
    if validation is not None:
        return RESULT_FAIL, validation
    if not expected_found(spec, text):
        return RESULT_FAIL, "expected output token was not found"
    if spec.unknown_on_pass:
        return RESULT_EVIDENCE_REQUIRED, "serial tokens matched; fixture/operator evidence needed"
    return RESULT_PASS, "matched expected serial evidence"


def command_timeout_s(command: str, default_s: float) -> float:
    parts = command.strip().split()
    if not parts:
        return default_s
    if parts[0] == "stress" and len(parts) >= 2:
        try:
            count = int(parts[1], 0)
        except ValueError:
            count = 0
        return max(default_s, 20.0 + (0.04 * max(count, 0)))
    if parts[0] == "stress_mix" and len(parts) >= 2:
        try:
            count = int(parts[1], 0)
        except ValueError:
            count = 0
        return max(default_s, 20.0 + (0.05 * max(count, 0)))
    if parts[0] in ("selftest",):
        return max(default_s, 30.0)
    return default_s


def base_plan() -> list[CommandSpec]:
    return [
        CommandSpec(
            "BOOT-001",
            "Connectivity",
            "version",
            "Firmware and library version output",
            ("=== Version Info ===", "ADS1115 library version"),
            ("firmware_clean_commit", "runtime_stack"),
            timeout_s=4.0,
        ),
        CommandSpec(
            "BOOT-002",
            "Connectivity",
            "scan",
            "I2C scan completes and finds at least one ADS1115-range address",
            ("Found", "device(s)"),
            ("scan_ads1115",),
            timeout_s=12.0,
            notes="Address ACK is not ADS1115 identity proof.",
        ),
    ]


def per_address_plan(address: str, *, full: bool, benchmark: bool) -> list[CommandSpec]:
    steps = [
        CommandSpec(
            "ADDR-{address}-001",
            "Address",
            "addr {address}",
            "Select initialized ADS1115 address {address}",
            ("Status: OK",),
            ("status_ok", "driver_ready"),
            timeout_s=8.0,
        ),
        CommandSpec(
            "ADDR-{address}-002",
            "Probe",
            "probe",
            "Probe CONFIG register without health side effects",
            ("Status: OK",),
            ("status_ok", "no_health_changes"),
            timeout_s=5.0,
            notes="ADS1115 has no chip-ID register.",
        ),
        CommandSpec(
            "ADDR-{address}-003",
            "Settings",
            "settings",
            "Cached settings show READY and clean hardware/cache state",
            ("=== Cached Settings ===",),
            ("driver_ready", "dirty_no"),
            timeout_s=5.0,
        ),
        CommandSpec(
            "ADDR-{address}-004",
            "Health",
            "drv",
            "Health is READY with zero failures",
            ("=== Driver Health ===",),
            ("driver_ready", "zero_failures"),
            timeout_s=5.0,
        ),
        CommandSpec(
            "ADDR-{address}-005",
            "Conversion",
            "read",
            "Blocking raw read returns raw and voltage",
            ("Raw:", "Voltage:"),
            ("raw_sample", "voltage"),
            timeout_s=12.0,
            unknown_on_pass=True,
            notes="Serial output is not calibrated analog accuracy evidence.",
        ),
        CommandSpec(
            "ADDR-{address}-006",
            "Selftest",
            "selftest",
            "Safe built-in selftest has zero failed checks",
            ("Selftest result:",),
            ("selftest",),
            timeout_s=35.0,
        ),
    ]
    if not full:
        return [step.formatted(address=address) for step in steps]

    full_steps = [
        CommandSpec("MUX-{address}-001", "Mux", "gain 2", "Set nominal +/-2.048 V PGA", ("Status: OK",), ("status_ok",)),
        CommandSpec("MUX-{address}-002", "Mux", "rate 4", "Set 128 SPS", ("Status: OK",), ("status_ok",)),
        CommandSpec("MUX-{address}-003", "Mux", "mode single", "Set single-shot mode", ("Status: OK",), ("status_ok",)),
    ]
    for channel in range(4):
        full_steps.append(CommandSpec(
            f"MUX-{{address}}-CH{channel}",
            "Mux",
            f"ch {channel}",
            f"Select AIN{channel}_GND",
            ("Status: OK",),
            ("status_ok",),
        ))
        full_steps.append(CommandSpec(
            f"MUX-{{address}}-CH{channel}-READ",
            "Mux",
            "read",
            f"Read AIN{channel}_GND",
            ("Raw:", "Voltage:"),
            ("raw_sample", "voltage"),
            timeout_s=12.0,
            unknown_on_pass=True,
        ))
    for diff in range(4):
        full_steps.append(CommandSpec(
            f"MUX-{{address}}-D{diff}",
            "Mux",
            f"diff {diff}",
            f"Select differential mux {diff}",
            ("Status: OK",),
            ("status_ok",),
        ))
        full_steps.append(CommandSpec(
            f"MUX-{{address}}-D{diff}-READ",
            "Mux",
            "readv",
            f"Read differential mux {diff} as voltage",
            ("Blocking voltage:", "Voltage:"),
            ("voltage",),
            timeout_s=12.0,
            unknown_on_pass=True,
        ))
    for gain in range(6):
        full_steps.append(CommandSpec(
            f"GAIN-{{address}}-{gain}",
            "Gain",
            f"gain {gain}",
            f"Set PGA enum {gain}",
            ("Status: OK",),
            ("status_ok",),
        ))
        full_steps.append(CommandSpec(
            f"GAIN-{{address}}-{gain}-READ",
            "Gain",
            "readv",
            f"Read voltage with PGA enum {gain}",
            ("Blocking voltage:", "Voltage:"),
            ("voltage",),
            timeout_s=12.0,
            unknown_on_pass=True,
        ))
    for rate in range(8):
        count = 4 if rate < 4 else 8
        full_steps.append(CommandSpec(
            f"RATE-{{address}}-{rate}",
            "Data Rate",
            f"rate {rate}",
            f"Set data-rate enum {rate}",
            ("Status: OK",),
            ("status_ok",),
        ))
        full_steps.append(CommandSpec(
            f"RATE-{{address}}-{rate}-TIMING",
            "Data Rate",
            "timing",
            f"Report timing for data-rate enum {rate}",
            ("Conversion time:", "LSB voltage:"),
        ))
        full_steps.append(CommandSpec(
            f"RATE-{{address}}-{rate}-STRESS",
            "Data Rate",
            f"stress {count}",
            f"Bounded stress at data-rate enum {rate}",
            ("=== Stress Summary ===",),
            ("stress_zero_errors", "rate_report"),
            timeout_s=25.0,
        ))
    full_steps.extend(
        [
            CommandSpec("MODE-{address}-001", "Mode", "mode single", "Restore single-shot mode", ("Status: OK",), ("status_ok",)),
            CommandSpec("MODE-{address}-002", "Mode", "start", "Start one single-shot conversion", ("Status:",), timeout_s=5.0),
            CommandSpec("MODE-{address}-003", "Mode", "poll", "Poll conversion readiness", ("Conversion ready:", "Status:"), timeout_s=5.0, unknown_on_pass=True),
            CommandSpec("MODE-{address}-004", "Mode", "read", "Read after single-shot path", ("Raw:", "Voltage:"), ("raw_sample", "voltage"), timeout_s=12.0, unknown_on_pass=True),
            CommandSpec("MODE-{address}-005", "Mode", "mode cont", "Set continuous mode", ("Status: OK",), ("status_ok",)),
            CommandSpec("MODE-{address}-006", "Mode", "raw", "Read latest raw in continuous mode", ("Raw:",), ("raw_sample",), timeout_s=8.0, unknown_on_pass=True),
            CommandSpec("MODE-{address}-007", "Mode", "voltage", "Reject scaled read in continuous mode", ("Status: UNSUPPORTED_OPERATION",), ("status_unsupported",), timeout_s=8.0, expected_failure=True),
            CommandSpec("MODE-{address}-008", "Mode", "mode single", "Restore single-shot mode", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-001", "Comparator", "comp", "Read comparator configuration", ("=== Comparator ===",), timeout_s=8.0),
            CommandSpec("COMP-{address}-002", "Comparator", "comp mode trad", "Set traditional comparator", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-003", "Comparator", "comp pol low", "Set active-low comparator", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-004", "Comparator", "comp latch 0", "Set non-latching comparator", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-005", "Comparator", "comp queue 1", "Set assert-after-one comparator queue", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-006", "Comparator", "comp th -1000 1000", "Set safe raw thresholds", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-007", "Comparator", "comp mode window", "Set window comparator", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-008", "Comparator", "comp pol high", "Set active-high comparator", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-009", "Comparator", "comp latch 1", "Set latching comparator", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-010", "Comparator", "comp queue 2", "Set assert-after-two comparator queue", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-011", "Comparator", "comp th -500 500", "Set alternate safe raw thresholds", ("Status: OK",), ("status_ok",)),
            CommandSpec("COMP-{address}-012", "Comparator", "comp rdy", "Enable conversion-ready threshold mode", ("Status: OK",), ("status_ok",), unknown_on_pass=True, notes="No ALERT/RDY instrument capture is performed by this runner."),
            CommandSpec("COMP-{address}-013", "Comparator", "comp disable", "Disable comparator output", ("Status: OK",), ("status_ok",)),
            CommandSpec("REG-{address}-001", "Registers", "reg 0", "Read conversion register", ("Reg 0x00",), timeout_s=5.0, unknown_on_pass=True),
            CommandSpec("REG-{address}-002", "Registers", "reg 1", "Read config register", ("Reg 0x01",), timeout_s=5.0),
            CommandSpec("REG-{address}-003", "Registers", "reg 2", "Read low threshold register", ("Reg 0x02",), timeout_s=5.0),
            CommandSpec("REG-{address}-004", "Registers", "reg 3", "Read high threshold register", ("Reg 0x03",), timeout_s=5.0),
            CommandSpec("DIRTY-{address}-001", "Dirty State", "wreg 1 0x8583", "Raw config write succeeds and marks cache dirty", ("Status: OK",), ("status_ok",), timeout_s=5.0),
            CommandSpec("DIRTY-{address}-002", "Dirty State", "settings", "Dirty state is visible after raw write", ("Hardware/cache dirty:",), ("dirty_yes",), timeout_s=5.0),
            CommandSpec("DIRTY-{address}-003", "Dirty State", "recover", "Recovery reapplies cached config", ("Status: OK",), ("status_ok",), timeout_s=8.0),
            CommandSpec("DIRTY-{address}-004", "Dirty State", "settings", "Dirty state clears after recovery", ("Hardware/cache dirty:",), ("dirty_no",), timeout_s=5.0),
            CommandSpec("JOB-{address}-001", "Staged Jobs", "job single", "Start poll-chunked single-shot job", ("Status:", "=== Job Status ==="), ("job_active",), timeout_s=5.0),
            CommandSpec("JOB-{address}-002", "Staged Jobs", "job poll 0", "Zero-budget poll consumes no transport instructions", ("=== Job Poll Result ===",), ("job_zero_budget",), timeout_s=5.0),
            CommandSpec("JOB-{address}-003", "Staged Jobs", "job poll 1", "One-instruction poll advances single-shot job", ("=== Job Poll Result ===",), timeout_s=5.0, post_delay_s=0.03),
            CommandSpec("JOB-{address}-004", "Staged Jobs", "job poll 3", "Full-budget poll completes single-shot job", ("=== Job Poll Result ===",), ("job_done",), timeout_s=8.0),
            CommandSpec("JOB-{address}-005", "Staged Jobs", "job apply", "Start poll-chunked config apply job", ("Status:", "=== Job Status ==="), ("job_active",), timeout_s=5.0),
            CommandSpec("JOB-{address}-006", "Staged Jobs", "job poll 0", "Zero-budget apply poll consumes no instructions", ("=== Job Poll Result ===",), ("job_zero_budget",), timeout_s=5.0),
            CommandSpec("JOB-{address}-007", "Staged Jobs", "job poll 1", "One-instruction apply poll advances", ("=== Job Poll Result ===",), timeout_s=5.0),
            CommandSpec("JOB-{address}-008", "Staged Jobs", "job poll 3", "First full-budget apply poll advances", ("=== Job Poll Result ===",), timeout_s=8.0),
            CommandSpec("JOB-{address}-009", "Staged Jobs", "job poll 3", "Second full-budget apply poll completes", ("=== Job Poll Result ===",), ("job_done",), timeout_s=8.0),
            CommandSpec("SHUT-{address}-001", "Lifecycle", "shutdown", "Shutdown writes single-shot idle and keeps initialized", ("Status: OK", "Mode:"), ("status_ok",), timeout_s=5.0),
            CommandSpec("SHUT-{address}-002", "Lifecycle", "settings", "Driver remains initialized after shutdown", ("Initialized: YES",), ("driver_ready",), timeout_s=5.0),
            CommandSpec("INV-{address}-001", "Invalid Input", "unknown_cmd", "Unknown command is rejected visibly", ("Unknown command:",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-002", "Invalid Input", "ch 4", "Invalid channel is rejected", ("Invalid channel",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-003", "Invalid Input", "gain 6", "Invalid gain is rejected", ("Invalid gain",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-004", "Invalid Input", "rate 8", "Invalid data rate is rejected", ("Invalid rate",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-005", "Invalid Input", "mode invalid", "Invalid mode is rejected", ("Invalid mode",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-006", "Invalid Input", "reg 4", "Invalid register read is rejected", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-007", "Invalid Input", "wreg 0 0x1234", "Invalid raw write register is rejected", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("INV-{address}-008", "Invalid Input", "job poll 999", "Invalid job budget is rejected", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
            CommandSpec("RESTORE-{address}-001", "Recovery", "recover", "Restore safe cached configuration after functional suite", ("Status: OK",), ("status_ok",), timeout_s=8.0),
            CommandSpec("RESTORE-{address}-002", "Health", "drv", "Final health remains READY with zero failures", ("=== Driver Health ===",), ("driver_ready", "zero_failures"), timeout_s=5.0),
        ]
    )
    if benchmark:
        full_steps.extend(
            [
                CommandSpec("BENCH-{address}-001", "Benchmark", "rate 7", "Set fastest data rate", ("Status: OK",), ("status_ok",)),
                CommandSpec("BENCH-{address}-002", "Benchmark", "stress 50", "Quick scalar read benchmark", ("=== Stress Summary ===",), ("stress_zero_errors", "rate_report"), timeout_s=25.0),
                CommandSpec("BENCH-{address}-003", "Benchmark", "stress 500", "Normal scalar read benchmark", ("=== Stress Summary ===",), ("stress_zero_errors", "rate_report"), timeout_s=60.0),
                CommandSpec("BENCH-{address}-004", "Benchmark", "stress_mix 200", "Mixed operation benchmark", ("=== stress_mix summary ===",), ("stress_mix_zero_fail", "rate_report"), timeout_s=60.0),
            ]
        )
    steps.extend(full_steps)
    return [step.formatted(address=address) for step in steps]


def targeted_address_plan(address: str) -> list[CommandSpec]:
    steps = [
        CommandSpec("TGT-{address}-ADDR", "Address", "addr {address}", "Select address {address}", ("Status: OK",), ("status_ok", "driver_ready"), timeout_s=8.0),
        CommandSpec("TGT-{address}-PROBE", "Probe", "probe", "Probe CONFIG register", ("Status: OK",), ("status_ok", "no_health_changes"), timeout_s=5.0, notes="ADS1115 has no chip-ID register."),
        CommandSpec("TGT-{address}-SETTINGS-0", "Settings", "settings", "Initial clean settings", ("=== Cached Settings ===",), ("driver_ready", "dirty_no"), timeout_s=5.0),
        CommandSpec("TGT-{address}-DRV-0", "Health", "drv", "Initial health is clean", ("=== Driver Health ===",), ("driver_ready", "zero_failures"), timeout_s=5.0),
        CommandSpec("TGT-{address}-RATE-0", "Data Rate", "rate 0", "Set slowest data-rate boundary", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-TIMING-0", "Data Rate", "timing", "Timing at slow boundary", ("Conversion time:", "LSB voltage:"), timeout_s=5.0),
        CommandSpec("TGT-{address}-RATE-7", "Data Rate", "rate 7", "Set fastest data-rate boundary", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-TIMING-7", "Data Rate", "timing", "Timing at fast boundary", ("Conversion time:", "LSB voltage:"), timeout_s=5.0),
        CommandSpec("TGT-{address}-RATE-4", "Data Rate", "rate 4", "Restore nominal data rate", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-GAIN-0", "Gain", "gain 0", "Set widest PGA boundary", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-GAIN-0-READ", "Gain", "readv", "Read at widest PGA", ("Blocking voltage:", "Voltage:"), ("voltage",), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-GAIN-5", "Gain", "gain 5", "Set narrowest PGA boundary", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-GAIN-5-READ", "Gain", "readv", "Read at narrowest PGA", ("Blocking voltage:", "Voltage:"), ("voltage",), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-GAIN-2", "Gain", "gain 2", "Restore nominal PGA", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MUX-CH0", "Mux", "ch 0", "Select first single-ended channel", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MUX-CH0-READ", "Mux", "read", "Read first single-ended channel", ("Raw:", "Voltage:"), ("raw_sample", "voltage"), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MUX-CH3", "Mux", "ch 3", "Select last single-ended channel", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MUX-CH3-READ", "Mux", "read", "Read last single-ended channel", ("Raw:", "Voltage:"), ("raw_sample", "voltage"), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MUX-D0", "Mux", "diff 0", "Select first differential mux", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MUX-D0-READ", "Mux", "readv", "Read first differential mux", ("Blocking voltage:", "Voltage:"), ("voltage",), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MUX-D3", "Mux", "diff 3", "Select last differential mux", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MUX-D3-READ", "Mux", "readv", "Read last differential mux", ("Blocking voltage:", "Voltage:"), ("voltage",), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MUX-RESTORE", "Mux", "ch 0", "Restore nominal mux", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MODE-SINGLE", "Mode", "mode single", "Set single-shot mode", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MODE-START", "Mode", "start", "Start single-shot conversion", ("Status:",), ("status_in_progress",), timeout_s=5.0),
        CommandSpec("TGT-{address}-MODE-POLL", "Mode", "poll", "Poll conversion readiness", ("Conversion ready:", "Status:"), timeout_s=5.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MODE-READ", "Mode", "read", "Read after single-shot start", ("Raw:", "Voltage:"), ("raw_sample", "voltage"), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MODE-CONT", "Mode", "mode cont", "Set continuous mode", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-MODE-RAW", "Mode", "raw", "Read latest raw code in continuous mode", ("Raw:",), ("raw_sample",), timeout_s=8.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-MODE-VOLTAGE-REJECT", "Mode", "voltage", "Reject scaled read in continuous mode", ("Status: UNSUPPORTED_OPERATION",), ("status_unsupported",), timeout_s=8.0, expected_failure=True),
        CommandSpec("TGT-{address}-MODE-RESTORE", "Mode", "mode single", "Restore single-shot mode", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-0", "Comparator", "comp", "Read comparator state", ("=== Comparator ===",), timeout_s=5.0),
        CommandSpec("TGT-{address}-COMP-TRAD", "Comparator", "comp mode trad", "Set traditional comparator", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-POL-LOW", "Comparator", "comp pol low", "Set active-low polarity", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-LATCH-0", "Comparator", "comp latch 0", "Set non-latching comparator", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-Q1", "Comparator", "comp queue 1", "Set assert-after-one queue", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-TH-EXTREME", "Comparator", "comp th -32768 32767", "Set int16 threshold extremes", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-WINDOW", "Comparator", "comp mode window", "Set window comparator", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-POL-HIGH", "Comparator", "comp pol high", "Set active-high polarity", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-LATCH-1", "Comparator", "comp latch 1", "Set latching comparator", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-Q4", "Comparator", "comp queue 4", "Set assert-after-four queue", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-COMP-RDY", "Comparator", "comp rdy", "Enable conversion-ready threshold mode", ("Status: OK",), ("status_ok",), unknown_on_pass=True, notes="No ALERT/RDY instrument capture is performed by this runner."),
        CommandSpec("TGT-{address}-COMP-1", "Comparator", "comp", "Read conversion-ready comparator state", ("=== Comparator ===",), timeout_s=5.0),
        CommandSpec("TGT-{address}-COMP-DISABLE", "Comparator", "comp disable", "Disable comparator output", ("Status: OK",), ("status_ok",)),
        CommandSpec("TGT-{address}-REG-0", "Registers", "reg 0", "Read conversion register", ("Reg 0x00",), timeout_s=5.0, unknown_on_pass=True),
        CommandSpec("TGT-{address}-REG-1", "Registers", "reg 1", "Read config register", ("Reg 0x01",), timeout_s=5.0),
        CommandSpec("TGT-{address}-REG-2", "Registers", "reg 2", "Read low threshold register", ("Reg 0x02",), timeout_s=5.0),
        CommandSpec("TGT-{address}-REG-3", "Registers", "reg 3", "Read high threshold register", ("Reg 0x03",), timeout_s=5.0),
        CommandSpec("TGT-{address}-CFG-WRITE", "Registers", "config write 0x8583", "Write full config register", ("Status: OK",), ("status_ok",), timeout_s=5.0),
        CommandSpec("TGT-{address}-CFG-READBACK", "Registers", "reg 1", "Read config after full write", ("Reg 0x01",), timeout_s=5.0),
        CommandSpec("TGT-{address}-DIRTY-WREG", "Dirty State", "wreg 2 0x8000", "Raw threshold write marks dirty", ("Status: OK",), ("status_ok",), timeout_s=5.0),
        CommandSpec("TGT-{address}-DIRTY-SETTINGS", "Dirty State", "settings", "Dirty state visible", ("Hardware/cache dirty:",), ("dirty_yes",), timeout_s=5.0),
        CommandSpec("TGT-{address}-DIRTY-RECOVER", "Recovery", "recover", "Recover clears raw-write dirty state", ("Status: OK",), ("status_ok",), timeout_s=8.0),
        CommandSpec("TGT-{address}-DIRTY-CLEAR", "Dirty State", "settings", "Dirty state cleared", ("Hardware/cache dirty:",), ("dirty_no",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-SINGLE", "Staged Jobs", "job single", "Start staged single-shot job", ("=== Job Status ===",), ("job_active",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-SINGLE-BUSY", "Staged Jobs", "job single", "Repeated single-shot job start is BUSY", ("Status:",), ("status_busy",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-POLL0", "Staged Jobs", "job poll 0", "Zero-budget poll consumes no instructions", ("=== Job Poll Result ===",), ("job_zero_budget",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-READ-BUSY", "Staged Jobs", "read", "Read is BUSY while staged job is active", ("Status:",), ("status_busy",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-POLL1", "Staged Jobs", "job poll 1", "One-instruction poll advances job", ("=== Job Poll Result ===",), timeout_s=5.0, post_delay_s=0.03),
        CommandSpec("TGT-{address}-JOB-POLL255", "Staged Jobs", "job poll 255", "Huge budget is clamped and completes single-shot job", ("=== Job Poll Result ===",), ("job_done",), timeout_s=8.0),
        CommandSpec("TGT-{address}-JOB-POLL-DONE", "Staged Jobs", "job poll", "Poll after complete reports no active job", ("No active pollable job",), ("no_active_job",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-APPLY", "Staged Jobs", "job apply", "Start staged config apply job", ("=== Job Status ===",), ("job_active",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-APPLY-BUSY", "Staged Jobs", "job apply", "Repeated apply job start is BUSY", ("Status:",), ("status_busy",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-APPLY-POLL0", "Staged Jobs", "job poll 0", "Zero-budget apply poll consumes no instructions", ("=== Job Poll Result ===",), ("job_zero_budget",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-CFG-BUSY", "Staged Jobs", "config write 0x8583", "Config write is BUSY while apply job is active", ("Status:",), ("status_busy",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-APPLY-POLL1", "Staged Jobs", "job poll 1", "One-instruction apply poll advances", ("=== Job Poll Result ===",), timeout_s=5.0),
        CommandSpec("TGT-{address}-JOB-APPLY-POLL255-1", "Staged Jobs", "job poll 255", "First huge budget is clamped and advances apply", ("=== Job Poll Result ===",), timeout_s=8.0),
        CommandSpec("TGT-{address}-JOB-APPLY-POLL255-2", "Staged Jobs", "job poll 255", "Second clamped poll completes apply", ("=== Job Poll Result ===",), ("job_done",), timeout_s=8.0),
        CommandSpec("TGT-{address}-STRESS-READ", "Stress", "stress 2", "Short scalar stress", ("=== Stress Summary ===",), ("stress_zero_errors", "rate_report"), timeout_s=25.0),
        CommandSpec("TGT-{address}-STRESS-MIX", "Stress", "stress_mix 3", "Short mixed stress", ("=== stress_mix summary ===",), ("stress_mix_zero_fail", "rate_report"), timeout_s=25.0),
        CommandSpec("TGT-{address}-INV-READ0", "Invalid Input", "read 0", "Reject zero read count", ("Invalid count",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-CH-NEG", "Invalid Input", "ch -1", "Reject negative channel", ("Invalid channel",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-CH4", "Invalid Input", "ch 4", "Reject channel above range", ("Invalid channel",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-DIFF4", "Invalid Input", "diff 4", "Reject differential mux above range", ("Invalid differential index",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-GAIN-NEG", "Invalid Input", "gain -1", "Reject negative gain", ("Invalid gain",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-GAIN6", "Invalid Input", "gain 6", "Reject gain above range", ("Invalid gain",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-RATE-NEG", "Invalid Input", "rate -1", "Reject negative data rate", ("Invalid rate",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-RATE8", "Invalid Input", "rate 8", "Reject data rate above range", ("Invalid rate",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-MODE", "Invalid Input", "mode invalid", "Reject invalid mode", ("Invalid mode",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-COMPQ3", "Invalid Input", "comp queue 3", "Reject unsupported comparator queue token", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-COMPTH", "Invalid Input", "comp th -32769 0", "Reject threshold below int16 range", ("Thresholds must be",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-CFG", "Invalid Input", "config write 0x10000", "Reject config write above uint16", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-REG4", "Invalid Input", "reg 4", "Reject register above range", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-WREG0", "Invalid Input", "wreg 0 0x1234", "Reject raw write to conversion register", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-JOB256", "Invalid Input", "job poll 256", "Reject poll budget above uint8", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-JOBNEG", "Invalid Input", "job poll -1", "Reject negative poll budget", ("Usage:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-INV-UNKNOWN", "Invalid Input", "unknown_cmd", "Reject unknown command", ("Unknown command:",), ("invalid_or_usage",), expected_failure=True),
        CommandSpec("TGT-{address}-RESTORE", "Recovery", "recover", "Final recovery restore", ("Status: OK",), ("status_ok",), timeout_s=8.0),
        CommandSpec("TGT-{address}-DRV-FINAL", "Health", "drv", "Final health remains clean", ("=== Driver Health ===",), ("driver_ready", "zero_failures"), timeout_s=5.0),
    ]
    return [step.formatted(address=address) for step in steps]


def exhaustive_diagnostic_plan(address: str) -> list[CommandSpec]:
    """Cover safe CLI surfaces not exercised by the functional plans."""
    steps = [
        CommandSpec("DIAG-{address}-HELP", "CLI", "help", "Render complete command help", ("ADS1115 Diagnostic Bring-up CLI Help",)),
        CommandSpec("DIAG-{address}-STATE", "Diagnostics", "state", "Render compact READY health", ("state=READY",), ("driver_ready",)),
        CommandSpec("DIAG-{address}-ADDR", "Address", "addr", "Report selected address", ("Active ADS1115 address:",)),
        CommandSpec("DIAG-{address}-CH", "Mux", "ch", "Report current mux as channel", ("Mux:",)),
        CommandSpec("DIAG-{address}-DIFF", "Mux", "diff", "Report current mux", ("Mux:",)),
        CommandSpec("DIAG-{address}-GAIN", "Gain", "gain", "Report current gain", ("Gain:",)),
        CommandSpec("DIAG-{address}-RATE", "Data Rate", "rate", "Report current data rate", ("Rate:",)),
        CommandSpec("DIAG-{address}-MODE", "Mode", "mode", "Report current operating mode", ("Mode:",)),
        CommandSpec("DIAG-{address}-CONFIG", "Registers", "config", "Read and decode CONFIG", ("Config:", "Fields:")),
        CommandSpec("DIAG-{address}-READ3", "Conversion", "read 3", "Read a bounded three-sample batch", ("3:",), ("batch_three",), timeout_s=15.0, unknown_on_pass=True),
        CommandSpec("DIAG-{address}-VERBOSE0", "CLI", "verbose", "Report initial verbose mode", ("Verbose mode:",)),
        CommandSpec("DIAG-{address}-VERBOSE1", "CLI", "verbose 1", "Enable verbose diagnostics", ("Verbose mode:",)),
        CommandSpec("DIAG-{address}-VERBOSE-RESTORE", "CLI", "verbose 0", "Restore quiet diagnostics", ("Verbose mode:",)),
        CommandSpec("DIAG-{address}-JOB0", "Staged Jobs", "job", "Report no active job", ("=== Job Status ===",), ("job_inactive",)),
        CommandSpec("DIAG-{address}-CANCEL-START", "Cancellation", "job single", "Start cancellable staged job", ("=== Job Status ===",), ("job_active",)),
        CommandSpec("DIAG-{address}-CANCEL", "Cancellation", "job cancel", "Cancel staged job and acknowledge terminal result", ("=== Job Status ===",), ("status_cancelled", "job_inactive"), expected_failure=True),
        CommandSpec("DIAG-{address}-COMP-OFF", "Comparator", "comp queue disable", "Disable comparator via queue command", ("Status: OK",), ("status_ok",)),
        CommandSpec("DIAG-{address}-WREG3", "Registers", "wreg 3 0x7FFF", "Write high threshold diagnostic register and mark dirty", ("Status: OK",), ("status_ok",)),
        CommandSpec("DIAG-{address}-DIRTY", "Dirty State", "settings", "High-threshold raw write is visibly dirty", ("Hardware/cache dirty:",), ("dirty_yes",)),
        CommandSpec("DIAG-{address}-RESTORE", "Recovery", "recover", "Restore verified configuration after diagnostics", ("Status: OK",), ("status_ok",), timeout_s=8.0),
        CommandSpec("DIAG-{address}-FINAL", "Health", "drv", "Finish READY with zero failures", ("=== Driver Health ===",), ("driver_ready", "zero_failures"), timeout_s=5.0),
    ]
    return [step.formatted(address=address) for step in steps]


def absent_address_plan(address: str) -> list[CommandSpec]:
    return [
        CommandSpec(
            f"ABSENT-{address.replace('0x', '')}-001",
            "Wrong Address",
            f"addr {address}",
            f"Absent address {address} reports visible error and preserves prior driver",
            ("Status:",),
            ("status_non_ok",),
            timeout_s=8.0,
            expected_failure=True,
            notes="Only run when this address is not physically present.",
        )
    ]


def soak_step_plan(addresses: list[str]) -> list[CommandSpec]:
    template = [
        CommandSpec("SOAK-{address}-ADDR", "Soak", "addr {address}", "Select address", ("Status: OK",), ("status_ok",), timeout_s=8.0),
        CommandSpec("SOAK-{address}-READ", "Soak", "read", "Blocking read", ("Raw:", "Voltage:"), ("raw_sample", "voltage"), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("SOAK-{address}-READV", "Soak", "readv", "Blocking voltage read", ("Blocking voltage:", "Voltage:"), ("voltage",), timeout_s=12.0, unknown_on_pass=True),
        CommandSpec("SOAK-{address}-CONT", "Soak", "mode cont", "Continuous mode", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-RAW", "Soak", "raw", "Continuous latest-raw read", ("Raw:",), ("raw_sample",), timeout_s=8.0, unknown_on_pass=True),
        CommandSpec("SOAK-{address}-SINGLE", "Soak", "mode single", "Single-shot restore", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-RATE0", "Soak", "rate 0", "Slow boundary rate", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-RATE0-READ", "Soak", "read", "Slow boundary read", ("Raw:", "Voltage:"), ("raw_sample", "voltage"), timeout_s=20.0, unknown_on_pass=True),
        CommandSpec("SOAK-{address}-RATE7", "Soak", "rate 7", "Fast boundary rate", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-STRESS", "Soak", "stress 20", "Short stress block", ("=== Stress Summary ===",), ("stress_zero_errors", "rate_report"), timeout_s=30.0),
        CommandSpec("SOAK-{address}-GAIN0", "Soak", "gain 0", "High range boundary", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-GAIN5", "Soak", "gain 5", "Low range boundary", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-NOMINAL-GAIN", "Soak", "gain 2", "Nominal range restore", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-NOMINAL-RATE", "Soak", "rate 4", "Nominal rate restore", ("Status: OK",), ("status_ok",)),
        CommandSpec("SOAK-{address}-JOB", "Soak", "job single", "Staged single-shot start", ("=== Job Status ===",), ("job_active",)),
        CommandSpec("SOAK-{address}-JOB-P1", "Soak", "job poll 1", "Staged single-shot first poll", ("=== Job Poll Result ===",), timeout_s=5.0, post_delay_s=0.03),
        CommandSpec("SOAK-{address}-JOB-P3", "Soak", "job poll 3", "Staged single-shot completion", ("=== Job Poll Result ===",), ("job_done",), timeout_s=8.0),
        CommandSpec("SOAK-{address}-SETTINGS", "Soak", "settings", "Settings snapshot", ("=== Cached Settings ===",), ("driver_ready",), timeout_s=5.0),
        CommandSpec("SOAK-{address}-DRV", "Soak", "drv", "Health snapshot", ("=== Driver Health ===",), ("driver_ready",), timeout_s=5.0),
        CommandSpec("SOAK-{address}-PROBE", "Soak", "probe", "Probe no health side effect", ("Status: OK",), ("status_ok", "no_health_changes"), timeout_s=5.0),
        CommandSpec("SOAK-{address}-RECOVER", "Soak", "recover", "Manual recovery path", ("Status: OK",), ("status_ok",), timeout_s=8.0),
    ]
    return [step.formatted(address=address) for address in addresses for step in template]


def build_plan(args: argparse.Namespace) -> list[CommandSpec]:
    addresses = args.address or ["0x48"]
    specs = base_plan()
    for address in addresses:
        if args.suite == "targeted":
            specs.extend(targeted_address_plan(address))
        else:
            specs.extend(per_address_plan(address, full=args.suite in ("full", "exhaustive"), benchmark=args.benchmark))
            if args.suite == "exhaustive":
                specs.extend(targeted_address_plan(address))
                specs.extend(exhaustive_diagnostic_plan(address))
    if args.absent_address:
        for address in args.absent_address:
            if address not in addresses:
                specs.extend(absent_address_plan(address))
        specs.append(CommandSpec("ABSENT-RESTORE", "Wrong Address", f"addr {addresses[0]}", "Restore first present address", ("Status: OK",), ("status_ok",)))
    return specs


def parser_self_test() -> None:
    host_short_commit = run_git(["rev-parse", "--short=7", "HEAD"])
    samples = [
        (CommandSpec("T", "Version", "version", "", ("=== Version Info ===",)), "=== Version Info ===\n> ", RESULT_PASS),
        (CommandSpec("T", "Health", "drv", "", ("=== Driver Health ===",), ("driver_ready", "zero_failures")), "=== Driver Health ===\n  State: READY\n  Total failures: 0\n> ", RESULT_PASS),
        (CommandSpec("T", "Read", "read", "", ("Raw:",), ("raw_sample", "voltage"), unknown_on_pass=True), "  Raw: -12\n  Voltage: -0.000750 V\n> ", RESULT_EVIDENCE_REQUIRED),
        (CommandSpec("T", "Probe", "probe", "", ("Status: OK",), ("status_ok", "no_health_changes")), "  Status: OK (code=0, detail=0)\n  Health changes:\n  (no health changes)\n> ", RESULT_PASS),
        (CommandSpec("T", "Scan", "scan", "", ("Scan complete",), ("scan_ads1115",)), "40: -- -- -- -- -- -- -- -- 48 49 -- -- -- -- -- --\nScan complete. Found 2 device(s).\n> ", RESULT_PASS),
        (CommandSpec("T", "Scan", "scan", "", ("Scan complete",), ("scan_ads1115",)), "30: -- -- -- -- -- -- -- -- -- -- -- -- 3C -- -- --\n40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\nScan complete. Found 1 device(s).\nCommon addresses: 0x48-0x4B=ADS1115\n> ", RESULT_FAIL),
        (CommandSpec("T", "Failure", "probe", "", ("Status:",), ("status_non_ok",), expected_failure=True), "  Status: I2C_TIMEOUT (code=7, detail=-1)\n> ", RESULT_PASS),
        (CommandSpec("T", "Job", "job poll 0", "", ("=== Job Poll Result ===",), ("job_zero_budget",)), "=== Job Poll Result ===\n  Status: IN_PROGRESS\n  Instructions used: 0\n  Done: NO\n> ", RESULT_PASS),
        (CommandSpec("T", "Job", "job single", "", ("Status:",), ("status_busy",)), "Status: BUSY (code=8, detail=0)\nMessage: Poll job active\n> ", RESULT_PASS),
        (CommandSpec("T", "Job", "job cancel", "", ("=== Job Status ===",), ("status_cancelled", "job_inactive"), expected_failure=True), "=== Job Status ===\n  Active: NO\n  Status: CANCELLED (code=19, detail=0)\n> ", RESULT_PASS),
        (CommandSpec("T", "Mode", "start", "", ("Status:",), ("status_in_progress",)), "Status: IN_PROGRESS (code=9, detail=0)\nMessage: Conversion started\n> ", RESULT_PASS),
        (CommandSpec("T", "Job", "job poll", "", ("No active pollable job",), ("no_active_job",)), "No active pollable job\n=== Job Status ===\n  Active: NO\n> ", RESULT_PASS),
        (CommandSpec("T", "Selftest", "selftest", "", ("Selftest result:",), ("selftest",)), "Selftest result: pass=18 fail=0 skip=0\n> ", RESULT_PASS),
        (CommandSpec("T", "Selftest", "selftest", "", ("Selftest result:",), ("selftest",)), "Selftest result: pass=18 fail=0 skip=1\n> ", RESULT_FAIL),
        (CommandSpec("T", "Status", "start", "", ("Status:",)), "Status: CONFIG_UNKNOWN (code=20, detail=0)\n> ", RESULT_FAIL),
        (CommandSpec("T", "Status", "poll", "", ("Status:",)), "Status: CONVERSION_NOT_READY (code=7, detail=0)\n> ", RESULT_FAIL),
        (CommandSpec("T", "Reset", "read", "", ("Raw:",), ("raw_sample",)), "=== ADS1115 Diagnostic Bring-up CLI ===\nReset reason: WDT\nRaw: 1\n> ", RESULT_FAIL),
        (CommandSpec("T", "Version", "version", "", ("ADS1115 library commit:",), ("firmware_clean_commit",)), f"ADS1115 library commit: {host_short_commit} (clean)\n> ", RESULT_PASS),
        (CommandSpec("T", "Version", "version", "", ("ADS1115 library commit:",), ("firmware_clean_commit",)), f"ADS1115 library commit: {host_short_commit} (dirty)\n> ", RESULT_FAIL),
        (CommandSpec("T", "Version", "version", "", ("Arduino-ESP32:",), ("runtime_stack",)), "Arduino-ESP32: 3.3.11\nESP-IDF: v5.5.5\n> ", RESULT_PASS),
        (CommandSpec("T", "Version", "version", "", ("Arduino-ESP32:",), ("runtime_stack",)), "Arduino-ESP32: 3.2.0\nESP-IDF: v5.4.1\n> ", RESULT_FAIL),
        (CommandSpec("T", "Stress", "stress 10", "", ("=== Stress Summary ===",), ("stress_zero_errors", "rate_report")), "=== Stress Summary ===\n  Errors: 0\n  Duration: 123 ms\n  Rate: 81.30 samples/s\n> ", RESULT_PASS),
        (CommandSpec("T", "Stress", "stress_mix 3", "", ("=== stress_mix summary ===",), ("stress_mix_zero_fail",)), "op=read ok=1 fail=0\n=== stress_mix summary ===\nTotal: ok=2 fail=1\n> ", RESULT_FAIL),
        (CommandSpec("T", "Stress", "stress_mix 3", "", ("=== stress_mix summary ===",), ("stress_mix_zero_fail",)), "=== stress_mix summary ===\nTotal: ok=3 fail=0\n> ", RESULT_PASS),
        (CommandSpec("T", "Stress", "stress_mix 3", "", ("=== stress_mix summary ===",), ("stress_mix_zero_fail",)), "=== stress_mix summary ===\nTotal: ok=1 fail=2\n> ", RESULT_FAIL),
        (CommandSpec("T", "Invalid", "ch 4", "", ("Invalid channel",), ("invalid_or_usage",), expected_failure=True), "Invalid channel\n> ", RESULT_PASS),
        (CommandSpec("T", "Invalid", "comp th -32769 0", "", ("Thresholds must be",), ("invalid_or_usage",), expected_failure=True), "Thresholds must be in int16 range\n> ", RESULT_PASS),
    ]
    for spec, output, expected in samples:
        result, reason = classify_output(spec, output)
        if result != expected:
            raise AssertionError(f"{spec.feature}: expected {expected}, got {result} ({reason})")
    targeted_plan = targeted_address_plan("0x48")
    targeted_ids = [spec.test_id for spec in targeted_plan]
    if len(targeted_ids) != len(set(targeted_ids)):
        raise AssertionError("targeted plan test IDs must be unique")
    targeted = {spec.test_id: spec for spec in targeted_plan}
    continuous = targeted.get("TGT-48-MODE-RAW")
    if continuous is None or continuous.command != "raw" or continuous.validators != ("raw_sample",):
        raise AssertionError("targeted continuous-mode contract must use latest raw diagnostics")
    scaled_reject = targeted.get("TGT-48-MODE-VOLTAGE-REJECT")
    if (scaled_reject is None or scaled_reject.command != "voltage" or
            scaled_reject.validators != ("status_unsupported",) or
            not scaled_reject.expected_failure):
        raise AssertionError("targeted scaled continuous read must be an explicit rejection test")
    apply_poll_1 = targeted.get("TGT-48-JOB-APPLY-POLL255-1")
    apply_poll_2 = targeted.get("TGT-48-JOB-APPLY-POLL255-2")
    if (apply_poll_1 is None or "job_done" in apply_poll_1.validators or
            apply_poll_2 is None or "job_done" not in apply_poll_2.validators):
        raise AssertionError("six-transfer staged apply must require both clamped polls")
    full_plan = per_address_plan("0x48", full=True, benchmark=False)
    full_ids = [spec.test_id for spec in full_plan]
    if len(full_ids) != len(set(full_ids)):
        raise AssertionError("full address plan test IDs must be unique")
    exhaustive_ids = full_ids + targeted_ids
    exhaustive_ids.extend(spec.test_id for spec in exhaustive_diagnostic_plan("0x48"))
    if len(exhaustive_ids) != len(set(exhaustive_ids)):
        raise AssertionError("exhaustive plan test IDs must be unique across full and targeted coverage")
    soak_ids = [spec.test_id for spec in soak_step_plan(["0x48"])]
    if len(soak_ids) != len(set(soak_ids)):
        raise AssertionError("soak plan test IDs must be unique")
    verdict_rows = [
        StepResult("V1", "Digital", "version", "", "", 0.0, RESULT_PASS, ""),
        StepResult("V2", "Analog", "read", "", "", 0.0, RESULT_EVIDENCE_REQUIRED, "",
                   evidence_required=True),
    ]
    if contract_verdict(verdict_rows, dry_run=False) != RESULT_PASS:
        raise AssertionError("contract verdict should ignore evidence-required rows")
    if evidence_verdict(verdict_rows, dry_run=False) != RESULT_EVIDENCE_REQUIRED:
        raise AssertionError("evidence verdict should surface missing evidence")
    if final_verdict(verdict_rows, dry_run=False) != RESULT_EVIDENCE_REQUIRED:
        raise AssertionError("final verdict should remain EVIDENCE_REQUIRED when evidence is missing")
    if process_exit_code(RESULT_UNKNOWN, fail_on_unknown=False) != 0:
        raise AssertionError("UNKNOWN should not fail exploratory runs")
    if process_exit_code(RESULT_UNKNOWN, fail_on_unknown=True) != 1:
        raise AssertionError("--fail-on-unknown should fail UNKNOWN final verdicts")
    if process_exit_code(RESULT_EVIDENCE_REQUIRED, fail_on_unknown=False) != 0:
        raise AssertionError("EVIDENCE_REQUIRED should not fail exploratory runs")
    if process_exit_code(RESULT_EVIDENCE_REQUIRED, fail_on_unknown=True) != 1:
        raise AssertionError("--fail-on-unknown should fail EVIDENCE_REQUIRED final verdicts")
    soak_start = dt.datetime.now()
    soak_with_failure = SoakStats(start=soak_start, end=soak_start)
    soak_with_failure.results.update({RESULT_PASS: 20, RESULT_FAIL: 1})
    soak_rows = soak_stats_to_rows(soak_with_failure)
    if soak_rows[0].result != RESULT_FAIL:
        raise AssertionError("any soak command failure must fail the soak summary")
    if not CLI_SYNC_BYTES.startswith(CLI_CANCEL_BYTE) or CLI_SYNC_BYTES.startswith(b"\n"):
        raise AssertionError("CLI synchronization must cancel, not dispatch, stale partial input")
    stale_prompt = "> \n=== Version Info ===\nArduino-ESP32: 3.3.11\n"
    if has_completed_cli_sync(stale_prompt):
        raise AssertionError("a stale prompt before the sync marker must not complete synchronization")
    completed_sync = "> \n=== Version Info ===\nArduino-ESP32: 3.3.11\n> "
    if not has_completed_cli_sync(completed_sync):
        raise AssertionError("the version marker and its following prompt must complete synchronization")
    print("ADS1115 HIL parser self-test PASSED")


def read_available(ser: object, idle_s: float, timeout_s: float) -> str:
    chunks: list[bytes] = []
    start = time.monotonic()
    last_data = start
    while True:
        waiting = int(getattr(ser, "in_waiting", 0))
        if waiting > 0:
            chunks.append(ser.read(waiting))
            last_data = time.monotonic()
        now = time.monotonic()
        if chunks and (now - last_data) >= idle_s:
            break
        if (now - start) >= timeout_s:
            break
        time.sleep(0.03)
    return b"".join(chunks).decode("utf-8", errors="replace")


def read_command_response(ser: object, idle_s: float, timeout_s: float) -> tuple[str, bool]:
    chunks: list[bytes] = []
    start = time.monotonic()
    last_data = start
    while True:
        waiting = int(getattr(ser, "in_waiting", 0))
        if waiting > 0:
            chunks.append(ser.read(waiting))
            last_data = time.monotonic()
            text = b"".join(chunks).decode("utf-8", errors="replace")
            if has_prompt(text):
                return text, False
        now = time.monotonic()
        if (now - start) >= timeout_s:
            break
        if chunks and (now - last_data) >= idle_s and has_prompt(
            b"".join(chunks).decode("utf-8", errors="replace")
        ):
            return b"".join(chunks).decode("utf-8", errors="replace"), False
        time.sleep(0.03)
    return b"".join(chunks).decode("utf-8", errors="replace"), True


def synchronize_cli(ser: object, timeout_s: float) -> tuple[str, bool]:
    """Cancel stale partial input and establish framing bus-silently."""
    chunks: list[bytes] = []
    ser.write(CLI_SYNC_BYTES)
    flush = getattr(ser, "flush", None)
    if callable(flush):
        flush()

    started_at = time.monotonic()
    while (time.monotonic() - started_at) < timeout_s:
        waiting = int(getattr(ser, "in_waiting", 0))
        if waiting > 0:
            chunks.append(ser.read(waiting))
            text = b"".join(chunks).decode("utf-8", errors="replace")
            if has_completed_cli_sync(text):
                return text, False
        time.sleep(0.03)
    return b"".join(chunks).decode("utf-8", errors="replace"), True


def open_serial(port: str, baud: int) -> object:
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("pyserial is required for live HIL; run dry-run or install pyserial") from exc
    ser = serial.Serial()
    try:
        ser.dtr = False
        ser.rts = False
    except (AttributeError, OSError):
        pass
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 0.1
    ser.write_timeout = 2.0
    ser.open()
    return ser


def reset_serial_target(ser: object) -> None:
    try:
        ser.dtr = False
        ser.rts = True
        time.sleep(0.1)
        ser.rts = False
        time.sleep(0.1)
    except Exception:
        return


def timestamp() -> str:
    return dt.datetime.now().strftime("%Y%m%d_%H%M%S")


def run_one_step(
    ser: object,
    spec: CommandSpec,
    *,
    idle_s: float,
    default_timeout_s: float,
    command_delay_s: float,
    write_log,
) -> StepResult:
    write_log(f"\n>>> {spec.command}\n")
    start = time.monotonic()
    try:
        ser.write((spec.command + "\r\n").encode("utf-8"))
        ser.flush()
        if command_delay_s > 0:
            time.sleep(command_delay_s)
        response, timed_out = read_command_response(
            ser,
            idle_s,
            max(default_timeout_s, spec.timeout_s, command_timeout_s(spec.command, default_timeout_s)),
        )
    except Exception as exc:
        elapsed = time.monotonic() - start
        reason = f"serial exception: {type(exc).__name__}: {exc}"
        write_log(f"# RESULT {spec.test_id}: {RESULT_FAIL} - {reason} ({elapsed:.3f}s)\n")
        return StepResult(
            test_id=spec.test_id,
            feature=spec.feature,
            command=spec.command,
            expected=spec.expected,
            observed="serial exception",
            elapsed_s=elapsed,
            result=RESULT_FAIL,
            notes=(spec.notes + ("; " if spec.notes else "") + reason).strip(),
            evidence_required=spec.unknown_on_pass,
        )
    elapsed = time.monotonic() - start
    if response:
        write_log(response)
    result, reason = classify_output(spec, response, timed_out=timed_out)
    observed = summarize_observed(response)
    write_log(f"# RESULT {spec.test_id}: {result} - {reason} ({elapsed:.3f}s)\n")
    if spec.post_delay_s > 0:
        time.sleep(spec.post_delay_s)
    return StepResult(
        test_id=spec.test_id,
        feature=spec.feature,
        command=spec.command,
        expected=spec.expected,
        observed=observed,
        elapsed_s=elapsed,
        result=result,
        notes=(spec.notes + ("; " if spec.notes and reason else "") + reason).strip(),
        evidence_required=spec.unknown_on_pass,
        reset_observed=any(marker in strip_ansi(response) for marker in REBOOT_MARKERS),
    )


def markdown_escape(text: str) -> str:
    return text.replace("|", "\\|").replace("\n", " ")


def write_markdown_summary(
    path: pathlib.Path,
    rows: list[StepResult],
    *,
    transcript_path: pathlib.Path | None,
    soak: SoakStats | None = None,
) -> None:
    counts = Counter(row.result for row in rows)
    verdict = final_verdict(rows, dry_run=False)
    contract = contract_verdict(rows, dry_run=False)
    evidence = evidence_verdict(rows, dry_run=False)
    with path.open("w", encoding="utf-8", newline="\n") as out:
        out.write("# ADS1115 HIL Runner Summary\n\n")
        out.write(f"- Generated: {dt.datetime.now().isoformat(timespec='seconds')}\n")
        out.write(f"- Branch: {run_git(['branch', '--show-current'])}\n")
        out.write(f"- Commit: {run_git(['rev-parse', 'HEAD'])}\n")
        if transcript_path is not None:
            out.write(f"- Transcript: `{transcript_path}`\n")
        out.write(
            f"- Results: PASS={counts[RESULT_PASS]} FAIL={counts[RESULT_FAIL]} "
            f"EVIDENCE_REQUIRED={counts[RESULT_EVIDENCE_REQUIRED]} "
            f"UNKNOWN={counts[RESULT_UNKNOWN]} NOT_RUN={counts[RESULT_NOT_RUN]}\n"
        )
        out.write(f"- Contract verdict: {contract}\n")
        out.write(f"- Evidence verdict: {evidence}\n")
        out.write(f"- Final verdict: {verdict}\n")
        reset_markers = sum(1 for row in rows if row.reset_observed)
        if soak is not None:
            reset_markers += soak.reset_markers
        out.write(f"- Unexpected reset/reboot markers after initial boot: {reset_markers}\n")
        if soak is not None:
            end = soak.end or dt.datetime.now()
            duration = (end - soak.start).total_seconds()
            out.write(f"- Soak duration: {duration:.1f} s\n")
            out.write(f"- Soak cycles: {soak.cycles}\n")
            out.write(f"- Soak stop reason: {soak.stopped_reason or 'completed requested duration'}\n")
            if soak.latencies:
                out.write(f"- Soak worst latency: {max(soak.latencies):.3f} s\n")
                out.write(f"- Soak mean latency: {sum(soak.latencies) / len(soak.latencies):.3f} s\n")
            out.write(f"- Soak worst read latency: {soak.worst_read_latency_s:.3f} s\n")
            out.write(f"- Soak max consecutive failures: {soak.max_consecutive_failures}\n")
        out.write("\n")
        out.write("| Test ID | Feature | Command | Expected | Observed | Elapsed s | Result | Evidence | Notes |\n")
        out.write("| --- | --- | --- | --- | --- | ---: | --- | --- | --- |\n")
        for row in rows:
            out.write(
                f"| {markdown_escape(row.test_id)} | {markdown_escape(row.feature)} | "
                f"`{markdown_escape(row.command)}` | {markdown_escape(row.expected)} | "
                f"{markdown_escape(row.observed)} | {row.elapsed_s:.3f} | "
                f"{row.result} | {'required' if row.evidence_required else ''} | "
                f"{markdown_escape(row.notes)} |\n"
            )


def print_plan(specs: Iterable[CommandSpec]) -> None:
    print("Command plan:")
    for index, spec in enumerate(specs, start=1):
        print(f"{index:03d}. [{spec.test_id}] {spec.command} - {spec.expected}")


def dry_run_rows(specs: Iterable[CommandSpec]) -> list[StepResult]:
    return [
        StepResult(
            test_id=spec.test_id,
            feature=spec.feature,
            command=spec.command,
            expected=spec.expected,
            observed="dry-run did not open serial",
            elapsed_s=0.0,
            result=RESULT_DRY_RUN,
            notes=spec.notes,
            evidence_required=spec.unknown_on_pass,
        )
        for spec in specs
    ]


def run_live(args: argparse.Namespace, specs: list[CommandSpec]) -> tuple[list[StepResult], pathlib.Path, pathlib.Path]:
    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    transcript_path = out_dir / f"ads1115_hil_{timestamp()}.log"
    summary_path = out_dir / f"ads1115_hil_{timestamp()}_summary.md"
    rows: list[StepResult] = []

    with open_serial(args.port, args.baud) as ser, transcript_path.open(
        "w", encoding="utf-8", newline="\n"
    ) as transcript:
        def write_log(text: str) -> None:
            transcript.write(text)
            transcript.flush()
            if args.verbose:
                sys.stdout.write(text)
                sys.stdout.flush()

        write_log(f"# ADS1115 serial HIL run {dt.datetime.now().isoformat(timespec='seconds')}\n")
        write_log(f"# Branch: {run_git(['branch', '--show-current'])}\n")
        write_log(f"# Commit: {run_git(['rev-parse', 'HEAD'])}\n")
        write_log(f"# Port: {args.port}, baud: {args.baud}, addresses: {', '.join(args.address or ['0x48'])}\n")
        write_log("# ADS1115 has no chip-ID register; probes are CONFIG-register reachability checks.\n\n")

        if args.reset_before:
            write_log("# Reset/reconnect requested before command plan.\n")
            reset_serial_target(ser)
        time.sleep(args.boot_settle_s)
        initial = read_available(ser, args.idle_s, args.timeout_s)
        if initial:
            write_log(initial)
        write_log(
            f"\n# Cancelling stale partial input and synchronizing CLI framing "
            f"with bus-silent `{CLI_SYNC_COMMAND}`.\n"
        )
        sync_output, sync_timed_out = synchronize_cli(ser, args.timeout_s)
        if sync_output:
            write_log(sync_output)
        if sync_timed_out:
            raise RuntimeError("serial CLI synchronization timed out")

        for spec in specs:
            row = run_one_step(
                ser,
                spec,
                idle_s=args.idle_s,
                default_timeout_s=args.timeout_s,
                command_delay_s=args.command_delay_s,
                write_log=write_log,
            )
            rows.append(row)
            if row.result == RESULT_FAIL and args.stop_on_fail:
                write_log("# Aborting after failed command because --stop-on-fail is set.\n")
                break

        soak_stats = None
        plan_failed = any(row.result == RESULT_FAIL for row in rows)
        if args.soak and not plan_failed:
            soak_stats = run_soak(ser, args, write_log)
            rows.extend(soak_stats_to_rows(soak_stats))
        elif args.soak:
            write_log("# SOAK NOT RUN because the prerequisite command plan failed.\n")
            rows.append(
                StepResult(
                    test_id="SOAK-SUMMARY",
                    feature="Bounded Soak",
                    command="--soak",
                    expected="Run only after the prerequisite command plan passes",
                    observed="not run",
                    elapsed_s=0.0,
                    result=RESULT_NOT_RUN,
                    notes="prerequisite command plan failed",
                )
            )

    write_markdown_summary(summary_path, rows, transcript_path=transcript_path, soak=soak_stats)
    return rows, transcript_path, summary_path


def run_soak(ser: object, args: argparse.Namespace, write_log) -> SoakStats:
    addresses = args.address or ["0x48"]
    specs = soak_step_plan(addresses)
    duration_s = args.soak_duration_s
    stats = SoakStats(start=dt.datetime.now())
    deadline = time.monotonic() + duration_s
    write_log(f"\n# SOAK START {stats.start.isoformat(timespec='seconds')} duration_s={duration_s}\n")
    while time.monotonic() < deadline:
        stats.cycles += 1
        for spec in specs:
            if time.monotonic() >= deadline:
                break
            row = run_one_step(
                ser,
                spec,
                idle_s=args.idle_s,
                default_timeout_s=args.timeout_s,
                command_delay_s=args.command_delay_s,
                write_log=write_log,
            )
            stats.commands[spec.command] += 1
            stats.results[row.result] += 1
            if row.reset_observed:
                stats.reset_markers += 1
            stats.latencies.append(row.elapsed_s)
            if spec.command in ("read", "readv", "raw", "voltage"):
                stats.worst_read_latency_s = max(stats.worst_read_latency_s, row.elapsed_s)
            if row.result == RESULT_FAIL:
                stats.consecutive_failures += 1
                stats.max_consecutive_failures = max(
                    stats.max_consecutive_failures,
                    stats.consecutive_failures,
                )
                if stats.consecutive_failures >= args.soak_max_consecutive_failures:
                    stats.stopped_reason = (
                        f"stopped after {stats.consecutive_failures} consecutive failures"
                    )
                    stats.end = dt.datetime.now()
                    write_log(f"# SOAK STOP {stats.stopped_reason}\n")
                    return stats
            else:
                stats.consecutive_failures = 0
    stats.end = dt.datetime.now()
    write_log(f"# SOAK END {stats.end.isoformat(timespec='seconds')}\n")
    return stats


def soak_stats_to_rows(stats: SoakStats) -> list[StepResult]:
    end = stats.end or dt.datetime.now()
    duration = (end - stats.start).total_seconds()
    failed_commands = stats.results[RESULT_FAIL]
    result = RESULT_FAIL if failed_commands else RESULT_PASS
    mean_latency = (sum(stats.latencies) / len(stats.latencies)) if stats.latencies else 0.0
    observed = (
        f"duration={duration:.1f}s cycles={stats.cycles} commands={sum(stats.commands.values())} "
        f"results={dict(stats.results)} worst={max(stats.latencies) if stats.latencies else 0.0:.3f}s "
        f"mean={mean_latency:.3f}s"
    )
    return [
        StepResult(
            test_id="SOAK-SUMMARY",
            feature="Bounded Soak",
            command="--soak",
            expected="Complete requested soak duration without command failures",
            observed=observed,
            elapsed_s=duration,
            result=result,
            notes=(
                stats.stopped_reason
                or (f"completed requested duration with {failed_commands} failure(s)" if failed_commands
                    else "completed requested duration")
            ),
        )
    ]


def final_verdict(rows: Iterable[StepResult], *, dry_run: bool) -> str:
    if dry_run:
        return RESULT_DRY_RUN
    results = [row.result for row in rows]
    if RESULT_FAIL in results:
        return RESULT_FAIL
    if RESULT_UNKNOWN in results:
        return RESULT_UNKNOWN
    if RESULT_EVIDENCE_REQUIRED in results:
        return RESULT_EVIDENCE_REQUIRED
    return RESULT_PASS


def contract_verdict(rows: Iterable[StepResult], *, dry_run: bool) -> str:
    if dry_run:
        return RESULT_DRY_RUN
    results = [row.result for row in rows]
    if RESULT_FAIL in results:
        return RESULT_FAIL
    if RESULT_UNKNOWN in results:
        return RESULT_UNKNOWN
    return RESULT_PASS


def evidence_verdict(rows: Iterable[StepResult], *, dry_run: bool) -> str:
    if dry_run:
        return RESULT_DRY_RUN
    evidence_rows = [row for row in rows if row.evidence_required]
    if not evidence_rows:
        return RESULT_NOT_RUN
    if any(row.result == RESULT_FAIL for row in evidence_rows):
        return RESULT_FAIL
    if any(row.result == RESULT_UNKNOWN for row in evidence_rows):
        return RESULT_UNKNOWN
    if any(row.result == RESULT_EVIDENCE_REQUIRED for row in evidence_rows):
        return RESULT_EVIDENCE_REQUIRED
    return RESULT_PASS


def process_exit_code(verdict: str, *, fail_on_unknown: bool) -> int:
    if verdict == RESULT_FAIL:
        return 1
    if fail_on_unknown and verdict in (RESULT_UNKNOWN, RESULT_EVIDENCE_REQUIRED):
        return 1
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, for example COM8 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--address", action="append", type=parse_address, help="ADS1115 address under test; repeat for multiple devices")
    parser.add_argument("--absent-address", action="append", type=parse_address, default=[], help="ADS1115 address expected to be absent for negative checks")
    parser.add_argument("--suite", choices=("smoke", "targeted", "full", "exhaustive"), default="smoke")
    parser.add_argument("--benchmark", action="store_true", help="Append bounded sampling benchmark steps")
    parser.add_argument("--timeout-s", "--timeout", dest="timeout_s", type=float, default=DEFAULT_TIMEOUT_S)
    parser.add_argument("--idle-s", "--idle", dest="idle_s", type=float, default=DEFAULT_IDLE_S)
    parser.add_argument("--boot-settle-s", type=float, default=DEFAULT_BOOT_SETTLE_S)
    parser.add_argument("--command-delay-s", type=float, default=0.0)
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    parser.add_argument("--verbose", action="store_true", help="Echo transcript while running")
    parser.add_argument("--stop-on-fail", action="store_true")
    parser.add_argument("--fail-on-unknown", "--fail-on-evidence-required",
                        dest="fail_on_unknown", action="store_true",
                        help="Return nonzero when final verdict is UNKNOWN or EVIDENCE_REQUIRED")
    parser.add_argument("--reset-before", action="store_true", help="Toggle serial reset lines before reading boot transcript")
    parser.add_argument("--soak", action="store_true", help="Run bounded soak loop after the command plan")
    parser.add_argument("--soak-duration-s", type=float, default=DEFAULT_SOAK_DURATION_S)
    parser.add_argument("--soak-max-consecutive-failures", type=int, default=3)
    parser.add_argument("--dry-run", action="store_true", help="Print the plan and run parser self-test")
    parser.add_argument("--parser-test", "--parser-self-test", action="store_true", help="Run parser/classifier tests only")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    specs = build_plan(args)

    if args.parser_test:
        parser_self_test()
        return 0

    print(f"Local branch: {run_git(['branch', '--show-current'])}")
    print(f"Local commit: {run_git(['rev-parse', 'HEAD'])}")
    print(f"Target ADS1115 addresses: {', '.join(args.address or ['0x48'])}")
    print_plan(specs)

    if args.dry_run:
        parser_self_test()
        rows = dry_run_rows(specs)
        verdict = final_verdict(rows, dry_run=True)
        contract = contract_verdict(rows, dry_run=True)
        evidence = evidence_verdict(rows, dry_run=True)
        print(f"Contract verdict: {contract}")
        print(f"Evidence verdict: {evidence}")
        print(f"Final verdict: {verdict}")
        return process_exit_code(verdict, fail_on_unknown=args.fail_on_unknown)

    if not args.port:
        print("--port is required unless --dry-run or --parser-test is used", file=sys.stderr)
        return 2

    rows, transcript_path, summary_path = run_live(args, specs)
    counts = Counter(row.result for row in rows)
    verdict = final_verdict(rows, dry_run=False)
    contract = contract_verdict(rows, dry_run=False)
    evidence = evidence_verdict(rows, dry_run=False)
    print(f"Saved transcript: {transcript_path}")
    print(f"Saved summary: {summary_path}")
    print(
        f"Counts: PASS={counts[RESULT_PASS]} FAIL={counts[RESULT_FAIL]} "
        f"EVIDENCE_REQUIRED={counts[RESULT_EVIDENCE_REQUIRED]} "
        f"UNKNOWN={counts[RESULT_UNKNOWN]} NOT_RUN={counts[RESULT_NOT_RUN]}"
    )
    print(f"Contract verdict: {contract}")
    print(f"Evidence verdict: {evidence}")
    print(f"Final verdict: {verdict}")
    return process_exit_code(verdict, fail_on_unknown=args.fail_on_unknown)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
