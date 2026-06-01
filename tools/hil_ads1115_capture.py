#!/usr/bin/env python3
"""Capture ADS1115 diagnostic CLI transcripts for HIL evidence.

This helper sends predefined command lists to the Arduino diagnostic CLI and
saves timestamped logs. It does not decide pass/fail; operators must review the
captured output and fill the hardware validation results template.
"""

from __future__ import annotations

import argparse
import datetime as dt
import pathlib
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
        "addr 0x49",
        "probe",
        "cfg",
        "addr 0x4A",
        "probe",
        "cfg",
        "addr 0x4B",
        "probe",
        "cfg",
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
            for name in SUITES:
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

        for command in commands:
            write_log(f"\n>>> {command}\n")
            ser.write((command + "\r\n").encode("utf-8"))
            ser.flush()
            time.sleep(per_command_delay_s)
            response = read_available(ser, quiet_s=quiet_s, max_s=read_timeout_s)
            if response:
                write_log(response)


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
    print("No pass/fail decision was made by this script.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
