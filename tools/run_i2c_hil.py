#!/usr/bin/env python3
"""Serial HIL runner for the ADS1115 diagnostic CLI.

The runner drives the existing Arduino diagnostic CLI. It does not flash
firmware, create a fake device, or prove analog accuracy by itself; it captures
bounded serial command results and classifies only what the CLI output supports.
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
from typing import Iterable


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_OUT = ROOT / "hil_logs"
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 8.0
DEFAULT_IDLE_S = 0.35

RESULT_PASS = "PASS"
RESULT_FAIL = "FAIL"
RESULT_TIMEOUT = "TIMEOUT"
RESULT_REVIEW = "REVIEW_REQUIRED"
RESULT_DRY_RUN = "DRY_RUN"

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
PROMPT_RE = re.compile(r"(^|\n)> ?$")
STATUS_FAILURE_RE = re.compile(
    r"\bStatus:\s*(DEVICE_NOT_FOUND|I2C_NACK_ADDR|I2C_NACK_DATA|"
    r"I2C_TIMEOUT|I2C_BUS|I2C_ERROR|TIMEOUT|INVALID_CONFIG|"
    r"INVALID_PARAM|NOT_INITIALIZED|OFFLINE|READBACK_MISMATCH)\b"
)
GENERIC_FAILURE_RE = (
    re.compile(r"\bFAILED\b"),
    re.compile(r"\bFAIL\b"),
    re.compile(r"\[E\]"),
)
RAW_SAMPLE_RE = re.compile(r"\bRaw:\s*-?\d+\b")
TOTAL_FAILURES_RE = re.compile(r"\bTotal failures:\s*(\d+)\b")


@dataclasses.dataclass(frozen=True)
class CommandSpec:
    name: str
    command: str
    purpose: str
    expected_any: tuple[str, ...]
    validators: tuple[str, ...] = ()
    failures: tuple[str, ...] = ()
    timeout_s: float = DEFAULT_TIMEOUT_S
    operator_check: bool = False
    notes: str = ""

    def formatted(self, *, address: str) -> "CommandSpec":
        return dataclasses.replace(self, command=self.command.format(address=address))


BASE_COMMANDS: tuple[CommandSpec, ...] = (
    CommandSpec(
        name="version",
        command="version",
        purpose="Capture firmware and ADS1115 library provenance.",
        expected_any=("=== Version Info ===", "ADS1115 library version"),
        timeout_s=4.0,
    ),
    CommandSpec(
        name="scan",
        command="scan",
        purpose="Capture I2C address ACK scan.",
        expected_any=("Scan complete", "device(s)"),
        timeout_s=12.0,
        notes="A scan proves only address ACK, not ADS1115 identity.",
    ),
    CommandSpec(
        name="address",
        command="addr {address}",
        purpose="Select the ADS1115 address under test.",
        expected_any=("Status: OK",),
        failures=("DEVICE_NOT_FOUND", "I2C_NACK_ADDR", "I2C_TIMEOUT", "I2C_BUS"),
        timeout_s=6.0,
    ),
    CommandSpec(
        name="probe",
        command="probe",
        purpose="Run raw CONFIG-register plausibility probe without health side effects.",
        expected_any=("Status: OK",),
        failures=("DEVICE_NOT_FOUND", "I2C_NACK_ADDR", "I2C_TIMEOUT", "I2C_BUS"),
        timeout_s=5.0,
        notes="ADS1115 has no chip-ID register; this is not identity proof.",
    ),
    CommandSpec(
        name="settings",
        command="settings",
        purpose="Record cached settings and dirty-state diagnostics.",
        expected_any=("=== Cached Settings ===", "Hardware/cache dirty:"),
        validators=("driver_ready",),
        timeout_s=5.0,
    ),
    CommandSpec(
        name="health",
        command="drv",
        purpose="Record health counters and last-error state.",
        expected_any=("=== Driver Health ===", "Total failures:"),
        validators=("driver_ready", "zero_failures"),
        timeout_s=5.0,
    ),
    CommandSpec(
        name="bounded-conversion",
        command="read",
        purpose="Run one bounded blocking conversion through the diagnostic CLI.",
        expected_any=("Raw:", "Voltage:"),
        validators=("raw_sample",),
        failures=("CONVERSION_NOT_READY", "TIMEOUT", "I2C_TIMEOUT", "I2C_BUS"),
        timeout_s=12.0,
        operator_check=True,
        notes="Serial success does not prove analog accuracy; compare input fixture separately.",
    ),
    CommandSpec(
        name="health-final",
        command="drv",
        purpose="Record final health state after the bounded conversion.",
        expected_any=("=== Driver Health ===", "Total failures:"),
        validators=("driver_ready", "zero_failures"),
        timeout_s=5.0,
    ),
)


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text).replace("\r", "")


def has_prompt(text: str) -> bool:
    return bool(PROMPT_RE.search(strip_ansi(text)))


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


def find_failure(spec: CommandSpec, text: str) -> str | None:
    plain = strip_ansi(text)
    if match := STATUS_FAILURE_RE.search(plain):
        return f"status failure token {match.group(1)}"
    for token in spec.failures:
        if token in plain:
            return f"command failure token {token}"
    for pattern in GENERIC_FAILURE_RE:
        if pattern.search(plain):
            return f"generic failure pattern {pattern.pattern}"
    return None


def expected_found(spec: CommandSpec, text: str) -> bool:
    plain = strip_ansi(text)
    return any(token in plain for token in spec.expected_any)


def validate_output(spec: CommandSpec, text: str) -> str | None:
    plain = strip_ansi(text)
    for validator in spec.validators:
        if validator == "driver_ready":
            if "State: READY" not in plain and "state=READY" not in plain:
                return "driver state is not READY"
        elif validator == "zero_failures":
            match = TOTAL_FAILURES_RE.search(plain)
            if not match:
                return "total failure count not found"
            if int(match.group(1)) != 0:
                return f"total failures={match.group(1)}"
        elif validator == "raw_sample":
            if RAW_SAMPLE_RE.search(plain) is None or "Voltage:" not in plain:
                return "raw sample and voltage were not both found"
        else:
            return f"unknown validator {validator}"
    return None


def classify_output(spec: CommandSpec, text: str, *, timed_out: bool = False) -> tuple[str, str]:
    if timed_out:
        return RESULT_TIMEOUT, "serial prompt was not observed before timeout"
    if failure := find_failure(spec, text):
        return RESULT_FAIL, failure
    if validation := validate_output(spec, text):
        return RESULT_FAIL, validation
    if not expected_found(spec, text):
        return RESULT_FAIL, "expected output token was not found"
    if spec.operator_check:
        return RESULT_REVIEW, "serial tokens matched; analog plausibility needs operator evidence"
    return RESULT_PASS, "matched expected serial tokens"


def parser_self_test() -> None:
    samples = [
        (
            BASE_COMMANDS[0],
            "=== Version Info ===\n  ADS1115 library version: 1.2.3\n> ",
            RESULT_PASS,
        ),
        (
            BASE_COMMANDS[4],
            "=== Cached Settings ===\n  State: READY\n  Hardware/cache dirty: NO\n> ",
            RESULT_PASS,
        ),
        (
            BASE_COMMANDS[5],
            "=== Driver Health ===\n  State: READY\n  Total failures: 0\n> ",
            RESULT_PASS,
        ),
        (
            BASE_COMMANDS[6],
            "  Raw: -12\n  Voltage: -0.000750 V\n> ",
            RESULT_REVIEW,
        ),
        (
            BASE_COMMANDS[3],
            "  Status: I2C_TIMEOUT (code=7, detail=-1)\n> ",
            RESULT_FAIL,
        ),
        (
            BASE_COMMANDS[5],
            "=== Driver Health ===\n  State: READY\n  Total failures: 2\n> ",
            RESULT_FAIL,
        ),
    ]
    for spec, output, expected in samples:
        result, reason = classify_output(spec, output)
        if result != expected:
            raise AssertionError(
                f"{spec.name}: expected {expected}, got {result} ({reason})"
            )
    print("ADS1115 HIL parser self-test PASSED")


def read_available(ser: object, idle_s: float, timeout_s: float) -> str:
    chunks: list[bytes] = []
    start = time.monotonic()
    last_data = start
    while True:
        waiting = int(getattr(ser, "in_waiting", 0))
        if waiting > 0:
            data = ser.read(waiting)
            chunks.append(data)
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
            data = ser.read(waiting)
            chunks.append(data)
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


def build_plan(address: str) -> list[CommandSpec]:
    return [spec.formatted(address=address) for spec in BASE_COMMANDS]


def dry_run_result(spec: CommandSpec) -> dict[str, str]:
    return {
        "name": spec.name,
        "command": spec.command,
        "result": RESULT_DRY_RUN,
        "reason": "dry-run did not open serial",
    }


def print_plan(specs: Iterable[CommandSpec]) -> None:
    print("Command plan:")
    for index, spec in enumerate(specs, start=1):
        mapping = f" ({spec.name})" if spec.name != spec.command else ""
        print(f"{index:02d}. {spec.command}{mapping} - {spec.purpose}")


def open_serial(port: str, baud: int) -> object:
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("pyserial is required for live HIL; run dry-run or install pyserial") from exc
    return serial.Serial(port=port, baudrate=baud, timeout=0.1)


def timestamp() -> str:
    return dt.datetime.now().strftime("%Y%m%d_%H%M%S")


def run_live(args: argparse.Namespace, specs: list[CommandSpec]) -> list[dict[str, str]]:
    out_dir = pathlib.Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    transcript_path = out_dir / f"ads1115_hil_{timestamp()}.log"
    rows: list[dict[str, str]] = []

    with open_serial(args.port, args.baud) as ser, transcript_path.open(
        "w", encoding="utf-8", newline="\n"
    ) as transcript:
        def write(text: str) -> None:
            transcript.write(text)
            transcript.flush()
            sys.stdout.write(text)
            sys.stdout.flush()

        write(f"# ADS1115 serial HIL run {dt.datetime.now().isoformat(timespec='seconds')}\n")
        write(f"# Branch: {run_git(['branch', '--show-current'])}\n")
        write(f"# Commit: {run_git(['rev-parse', 'HEAD'])}\n")
        write(f"# Port: {args.port}, baud: {args.baud}, address: {args.address}\n")
        write("# Results are serial evidence inputs; operator review remains required.\n\n")

        time.sleep(2.0)
        initial = read_available(ser, args.idle, args.timeout)
        if initial:
            write(initial)

        for spec in specs:
            write(f"\n>>> {spec.command}\n")
            ser.write((spec.command + "\r\n").encode("utf-8"))
            ser.flush()
            response, timed_out = read_command_response(
                ser, args.idle, max(args.timeout, spec.timeout_s)
            )
            if response:
                write(response)
            result, reason = classify_output(spec, response, timed_out=timed_out)
            write(f"# RESULT {spec.name}: {result} - {reason}\n")
            rows.append(
                {
                    "name": spec.name,
                    "command": spec.command,
                    "result": result,
                    "reason": reason,
                }
            )
            if result in (RESULT_FAIL, RESULT_TIMEOUT):
                write("# Aborting after failed command to avoid overlapping diagnostics.\n")
                break

    print(f"\nSaved transcript: {transcript_path}")
    return rows


def final_verdict(rows: Iterable[dict[str, str]], *, dry_run: bool) -> str:
    if dry_run:
        return RESULT_DRY_RUN
    results = [row["result"] for row in rows]
    if any(result in (RESULT_FAIL, RESULT_TIMEOUT) for result in results):
        return RESULT_FAIL
    if RESULT_REVIEW in results:
        return RESULT_REVIEW
    return RESULT_PASS


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, for example COM19 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--address", type=parse_address, default="0x48")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT_S)
    parser.add_argument("--idle", type=float, default=DEFAULT_IDLE_S)
    parser.add_argument("--out", default=str(DEFAULT_OUT))
    parser.add_argument("--dry-run", action="store_true", help="Print the plan and run parser tests")
    parser.add_argument("--parser-test", action="store_true", help="Run parser/classifier tests only")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    specs = build_plan(args.address)

    if args.parser_test:
        parser_self_test()
        return 0

    print(f"Local branch: {run_git(['branch', '--show-current'])}")
    print(f"Local commit: {run_git(['rev-parse', 'HEAD'])}")
    print(f"Target ADS1115 address: {args.address}")
    print_plan(specs)

    if args.dry_run:
        parser_self_test()
        rows = [dry_run_result(spec) for spec in specs]
        verdict = final_verdict(rows, dry_run=True)
        print(f"Final verdict: {verdict}")
        return 0

    if not args.port:
        print("--port is required unless --dry-run or --parser-test is used", file=sys.stderr)
        return 2

    rows = run_live(args, specs)
    verdict = final_verdict(rows, dry_run=False)
    print(f"Final verdict: {verdict}")
    return 1 if verdict == RESULT_FAIL else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
