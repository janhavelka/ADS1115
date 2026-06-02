#!/usr/bin/env python3
"""Capture ADS1115 diagnostic CLI transcripts for HIL evidence.

This helper sends predefined command lists to the Arduino diagnostic CLI and
saves timestamped logs. It performs basic sequencing checks so functional
commands are only sent after a READY initialized address. It does not replace
operator review; operators must still review captured output and fill the
hardware validation results template.
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import re
import subprocess
import sys
import time
from typing import Iterable, List


ROOT = pathlib.Path(__file__).resolve().parents[1]

SUITES = {
    "identity": [
        "version",
        "addr",
        "state",
        "cfg",
        "drv",
    ],
    "address": [
        "addr 0x48",
        "probe",
        "cfg",
        "selftest",
        "addr 0x49",
        "probe",
        "cfg",
        "selftest",
        "addr 0x4A",
        "probe",
        "cfg",
        "addr 0x48",
        "probe",
        "cfg",
        "selftest",
        "addr 0x4B",
        "probe",
        "cfg",
        "addr 0x48",
        "probe",
        "cfg",
        "selftest",
    ],
    "mux": [
        "gain 2",
        "rate 4",
        "mode single",
        "ch 0",
        "read",
        "readv",
        "ch 1",
        "read",
        "readv",
        "ch 2",
        "read",
        "readv",
        "ch 3",
        "read",
        "readv",
        "diff 0",
        "read",
        "readv",
        "diff 1",
        "read",
        "readv",
        "diff 2",
        "read",
        "readv",
        "diff 3",
        "read",
        "readv",
    ],
    "gain": [
        "gain 0",
        "readv",
        "gain 1",
        "readv",
        "gain 2",
        "readv",
        "gain 3",
        "readv",
        "gain 4",
        "readv",
        "gain 5",
        "readv",
        "timing",
    ],
    "rate": [
        "rate 0",
        "timing",
        "stress 20",
        "rate 1",
        "timing",
        "stress 20",
        "rate 2",
        "timing",
        "stress 20",
        "rate 3",
        "timing",
        "stress 20",
        "rate 4",
        "timing",
        "stress 50",
        "rate 5",
        "timing",
        "stress 50",
        "rate 6",
        "timing",
        "stress 100",
        "rate 7",
        "timing",
        "stress 100",
    ],
    "mode": [
        "mode single",
        "start",
        "poll",
        "read",
        "readv",
        "mode cont",
        "poll",
        "raw",
        "voltage",
        "mode single",
    ],
    "comparator": [
        "comp",
        "comp mode trad",
        "comp pol low",
        "comp latch 0",
        "comp queue 1",
        "comp",
        "comp mode window",
        "comp pol high",
        "comp latch 1",
        "comp queue 2",
        "comp",
        "comp rdy",
        "comp",
        "comp disable",
    ],
    "fault": [
        "drv",
        "recover",
        "drv",
        "wreg 1 0x8583",
        "cfg",
        "recover",
        "cfg",
    ],
    "stress": [
        "version",
        "addr",
        "cfg",
        "stress 1000",
        "stress_mix 200",
        "drv",
    ],
}

RESTORE_COMMANDS = ["addr 0x48", "probe", "cfg", "selftest"]

NON_FUNCTIONAL_PREFIXES = (
    "addr",
    "cfg",
    "drv",
    "probe",
    "recover",
    "state",
    "version",
)


def run_git(args: List[str]) -> str:
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


def build_commands(suites: Iterable[str], extra: Iterable[str]) -> List[str]:
    commands: List[str] = []
    for suite in suites:
        if suite == "all":
            commands.extend(SUITES["identity"])
            commands.extend(SUITES["address"])
            for name in ("mux", "gain", "rate", "mode", "comparator", "fault", "stress"):
                commands.append("cfg")
                commands.extend(SUITES[name])
            continue
        commands.extend(SUITES[suite])
    commands.extend(extra)
    return commands


def render_command_list(commands: List[str]) -> str:
    return "\n".join(commands) + ("\n" if commands else "")


def timestamp() -> str:
    return dt.datetime.now().strftime("%Y%m%d_%H%M%S")


def read_available(ser, quiet_s: float, max_s: float) -> str:
    chunks: List[bytes] = []
    start = time.monotonic()
    last_data = start
    while True:
        waiting = getattr(ser, "in_waiting", 0)
        if waiting:
            data = ser.read(waiting)
            chunks.append(data)
            last_data = time.monotonic()
        now = time.monotonic()
        if chunks and (now - last_data) >= quiet_s:
            break
        if (now - start) >= max_s:
            break
        time.sleep(0.05)
    return b"".join(chunks).decode("utf-8", errors="replace")


def ansi_stripped(text: str) -> str:
    return re.sub(r"\x1b\[[0-9;]*m", "", text)


def response_is_ready(response: str) -> bool:
    plain = ansi_stripped(response)
    if "Address note:" in plain or "Requested address is not initialized" in plain:
        return False
    if "Initialized: YES" in plain and "State: READY" in plain:
        return True
    if "State: READY" in plain and "Online: yes" in plain:
        return True
    return False


def response_is_not_ready(response: str) -> bool:
    plain = ansi_stripped(response)
    return (
        "NOT_INITIALIZED" in plain
        or "State: UNINIT" in plain
        or "Address note:" in plain
        or "Requested address is not initialized" in plain
    )


def selftest_ok(response: str) -> bool:
    plain = ansi_stripped(response)
    match = re.search(r"Selftest result:\s+pass=(\d+)\s+fail=(\d+)\s+skip=(\d+)", plain)
    if not match:
        return "Selftest result:" not in plain
    return int(match.group(2)) == 0


def command_is_functional(command: str) -> bool:
    token = command.strip().split(" ", 1)[0]
    return token not in NON_FUNCTIONAL_PREFIXES


def address_command_target(command: str) -> str | None:
    parts = command.strip().split()
    if len(parts) != 2 or parts[0] != "addr":
        return None
    return parts[1]


def classify_address_response(command: str, response: str) -> str | None:
    target = address_command_target(command)
    if target is None:
        return None
    plain = ansi_stripped(response)
    if "Status: OK" in plain:
        return f"# Address check {target}: present/pass (initialized OK)\n"
    status_match = re.search(r"Status:\s+([A-Z0-9_]+)", plain)
    if status_match:
        if target.lower() in ("0x4a", "0x4b"):
            return (
                f"# Address check {target}: absent/pass-as-negative-test "
                f"({status_match.group(1)})\n"
            )
        return f"# Address check {target}: unavailable/review-expected-wiring ({status_match.group(1)})\n"
    return f"# Address check {target}: no status parsed; review manually\n"


def capture_serial(port: str, baud: int, commands: List[str], out_path: pathlib.Path,
                   per_command_delay_s: float, quiet_s: float, read_timeout_s: float) -> None:
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit(
            "pyserial is required for live capture; install it or use --dry-run"
        ) from exc

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with serial.Serial(port=port, baudrate=baud, timeout=0.1) as ser, out_path.open(
        "w", encoding="utf-8", newline="\n"
    ) as log:
        def write_log(text: str) -> None:
            log.write(text)
            log.flush()
            sys.stdout.write(text)
            sys.stdout.flush()

        write_log(f"# ADS1115 HIL capture {dt.datetime.now().isoformat(timespec='seconds')}\n")
        write_log(f"# Local branch: {run_git(['branch', '--show-current'])}\n")
        write_log(f"# Local commit: {run_git(['rev-parse', 'HEAD'])}\n")
        write_log(f"# Serial port: {port}, baud: {baud}\n")
        write_log("# This transcript is evidence only; review manually for pass/fail.\n\n")

        time.sleep(2.0)
        initial = read_available(ser, quiet_s=quiet_s, max_s=read_timeout_s)
        if initial:
            write_log(initial)

        current_ready = response_is_ready(initial) if initial else False
        restore_attempted_for_command = False

        def send_command(command: str) -> str:
            write_log(f"\n>>> {command}\n")
            ser.write((command + "\r\n").encode("utf-8"))
            ser.flush()
            time.sleep(per_command_delay_s)
            response = read_available(ser, quiet_s=quiet_s, max_s=read_timeout_s)
            if response:
                write_log(response)
            return response

        def restore_ready() -> bool:
            nonlocal current_ready
            write_log("\n# Restore precondition: addr 0x48, probe, cfg, selftest\n")
            restored = False
            selftest_passed = True
            for restore_command in RESTORE_COMMANDS:
                response = send_command(restore_command)
                if restore_command == "cfg":
                    restored = response_is_ready(response)
                if restore_command == "selftest":
                    selftest_passed = selftest_ok(response)
            current_ready = restored and selftest_passed
            if not current_ready:
                write_log("# Restore failed; aborting HIL capture before functional commands.\n")
            return current_ready

        for command in commands:
            if command.strip() == "selftest" and not current_ready:
                write_log(
                    "# Selftest precondition failed for the requested address; "
                    "restoring 0x48 instead.\n"
                )
                if not restore_ready():
                    raise SystemExit(2)
                continue

            if command_is_functional(command) and not current_ready:
                if restore_attempted_for_command:
                    write_log("# Functional precondition still failed after restore; aborting.\n")
                    raise SystemExit(2)
                restore_attempted_for_command = True
                if not restore_ready():
                    raise SystemExit(2)
            else:
                restore_attempted_for_command = False

            response = send_command(command)
            address_note = classify_address_response(command, response)
            if address_note:
                write_log(address_note)
            if command == "cfg":
                current_ready = response_is_ready(response)
            elif response_is_not_ready(response):
                current_ready = False
            elif response_is_ready(response):
                current_ready = True


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port, for example COM5 or /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--out-dir", default="hil_logs")
    parser.add_argument(
        "--suite",
        action="append",
        choices=[*SUITES.keys(), "all"],
        default=[],
        help="Command suite to run; may be repeated",
    )
    parser.add_argument("--command", action="append", default=[], help="Extra command to append")
    parser.add_argument("--dry-run", action="store_true", help="Print commands without opening serial")
    parser.add_argument("--list-suites", action="store_true", help="List available suites and exit")
    parser.add_argument("--per-command-delay-s", type=float, default=0.3)
    parser.add_argument("--quiet-s", type=float, default=0.5)
    parser.add_argument("--read-timeout-s", type=float, default=8.0)
    args = parser.parse_args(argv)

    if args.list_suites:
        for name, commands in SUITES.items():
            print(f"{name}: {len(commands)} commands")
        return 0

    suites = args.suite or ["identity"]
    commands = build_commands(suites, args.command)

    print(f"Local branch: {run_git(['branch', '--show-current'])}")
    print(f"Local commit: {run_git(['rev-parse', 'HEAD'])}")
    print("Device identity command included when using the default identity suite: version")
    print("Commands:")
    print(render_command_list(commands), end="")

    if args.dry_run:
        return 0

    if not args.port:
        parser.error("--port is required unless --dry-run is used")

    out_dir = pathlib.Path(args.out_dir)
    out_path = out_dir / f"ads1115_hil_{timestamp()}.log"
    capture_serial(
        port=args.port,
        baud=args.baud,
        commands=commands,
        out_path=out_path,
        per_command_delay_s=args.per_command_delay_s,
        quiet_s=args.quiet_s,
        read_timeout_s=args.read_timeout_s,
    )
    print(f"\nSaved transcript: {out_path}")
    print("Basic sequencing/address classification was recorded; operator review is still required.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
