#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import re
import sys
from typing import Dict

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCAN_DIRS = ("src", "include")
VALID_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

FORBIDDEN_CALLS = {
    "millis": re.compile(r"\bmillis\s*\("),
    "micros": re.compile(r"\bmicros\s*\("),
    "delay": re.compile(r"\bdelay\s*\("),
    "delayMicroseconds": re.compile(r"\bdelayMicroseconds\s*\("),
    "yield": re.compile(r"\byield\s*\("),
    "vTaskDelay": re.compile(r"\bvTaskDelay\s*\("),
    "malloc": re.compile(r"\bmalloc\s*\("),
    "calloc": re.compile(r"\bcalloc\s*\("),
    "realloc": re.compile(r"\brealloc\s*\("),
    "free": re.compile(r"\bfree\s*\("),
    "ESP_LOG": re.compile(r"\bESP_LOG[A-Z]?\s*\("),
    "Serial.print": re.compile(r"\bSerial\s*\.\s*(?:print|println|printf)\s*\("),
}

FORBIDDEN_SYMBOLS = {
    "TwoWire": re.compile(r"\bTwoWire\b"),
    "Serial": re.compile(r"\bSerial\b"),
    "String": re.compile(r"\bString\b"),
    "Print": re.compile(r"\bPrint\b"),
    "Stream": re.compile(r"\bStream\b"),
    "esp_err_t": re.compile(r"\besp_err_t\b"),
    "SemaphoreHandle_t": re.compile(r"\bSemaphoreHandle_t\b"),
    "std::vector": re.compile(r"\bstd\s*::\s*vector\b"),
    "std::string": re.compile(r"\bstd\s*::\s*string\b"),
    "std::function": re.compile(r"\bstd\s*::\s*function\b"),
    "new": re.compile(r"\bnew\s+"),
    "delete": re.compile(r"\bdelete\s+(?:\[\s*\]\s*)?"),
}

FORBIDDEN_INCLUDES = {
    "Arduino.h": re.compile(r'^\s*#\s*include\s*[<"]Arduino\.h[>"]', re.MULTILINE),
    "Wire.h": re.compile(r'^\s*#\s*include\s*[<"]Wire\.h[>"]', re.MULTILINE),
    "FreeRTOS": re.compile(r'^\s*#\s*include\s*[<"][^>"]*(?:FreeRTOS|freertos)/[^>"]*[>"]', re.MULTILINE),
    "ESP-IDF driver": re.compile(r'^\s*#\s*include\s*[<"]driver/[^>"]+[>"]', re.MULTILINE),
    "ESP-IDF esp_*": re.compile(r'^\s*#\s*include\s*[<"]esp_[^>"]+[>"]', re.MULTILINE),
    "ESP-IDF sdkconfig": re.compile(r'^\s*#\s*include\s*[<"]sdkconfig\.h[>"]', re.MULTILINE),
    "ESP-IDF soc": re.compile(r'^\s*#\s*include\s*[<"]soc/[^>"]+[>"]', re.MULTILINE),
    "ESP-IDF hal": re.compile(r'^\s*#\s*include\s*[<"]hal/[^>"]+[>"]', re.MULTILINE),
    "ESP-IDF rom": re.compile(r'^\s*#\s*include\s*[<"]rom/[^>"]+[>"]', re.MULTILINE),
    "ESP-IDF lwip": re.compile(r'^\s*#\s*include\s*[<"]lwip/[^>"]+[>"]', re.MULTILINE),
    "ESP-IDF nvs_flash": re.compile(r'^\s*#\s*include\s*[<"]nvs_flash\.h[>"]', re.MULTILINE),
}
BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.DOTALL)
LINE_COMMENT_RE = re.compile(r"//[^\n]*")
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')

ALLOWED_CALL_COUNTS: Dict[str, Dict[str, int]] = {}
ALLOWED_SYMBOL_COUNTS: Dict[str, Dict[str, int]] = {}
ALLOWED_INCLUDE_COUNTS: Dict[str, Dict[str, int]] = {}


def strip_comments(text: str) -> str:
    text = BLOCK_COMMENT_RE.sub("", text)
    return LINE_COMMENT_RE.sub("", text)


def strip_non_code(text: str) -> str:
    return STRING_RE.sub('""', strip_comments(text))


def collect_sources() -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for dirname in SCAN_DIRS:
        root = ROOT / dirname
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in VALID_SUFFIXES:
                files.append(path)
    return files


def main() -> int:
    observed_calls: Dict[str, Dict[str, int]] = {}
    observed_symbols: Dict[str, Dict[str, int]] = {}
    observed_includes: Dict[str, Dict[str, int]] = {}

    for path in collect_sources():
        rel = path.relative_to(ROOT).as_posix()
        raw = path.read_text(encoding="utf-8", errors="replace")
        code = strip_non_code(raw)
        include_text = strip_comments(raw)

        call_counts: Dict[str, int] = {}
        for call_name, pattern in FORBIDDEN_CALLS.items():
            count = len(pattern.findall(code))
            if count > 0:
                call_counts[call_name] = count
        if call_counts:
            observed_calls[rel] = call_counts

        symbol_counts: Dict[str, int] = {}
        for symbol_name, pattern in FORBIDDEN_SYMBOLS.items():
            count = len(pattern.findall(code))
            if count > 0:
                symbol_counts[symbol_name] = count
        if symbol_counts:
            observed_symbols[rel] = symbol_counts

        include_counts: Dict[str, int] = {}
        for include_name, pattern in FORBIDDEN_INCLUDES.items():
            count = len(pattern.findall(include_text))
            if count > 0:
                include_counts[include_name] = count
        if include_counts:
            observed_includes[rel] = include_counts

    errors: list[str] = []

    for rel, counts in observed_calls.items():
        if rel not in ALLOWED_CALL_COUNTS:
            errors.append(f"forbidden timing calls in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_CALL_COUNTS[rel]
        for call_name, count in counts.items():
            exp = expected.get(call_name, 0)
            if count != exp:
                errors.append(
                    f"timing call count mismatch in {rel}: {call_name} observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_CALL_COUNTS.items():
        observed = observed_calls.get(rel, {})
        for call_name, exp in expected.items():
            obs = observed.get(call_name, 0)
            if obs != exp:
                errors.append(
                    f"timing call count mismatch in {rel}: {call_name} observed={obs}, expected={exp}"
                )
        unexpected_calls = set(observed.keys()) - set(expected.keys())
        if unexpected_calls:
            errors.append(f"unexpected timing call types in {rel}: {sorted(unexpected_calls)}")

    for rel, counts in observed_symbols.items():
        if rel not in ALLOWED_SYMBOL_COUNTS:
            errors.append(f"forbidden framework/allocation symbols in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_SYMBOL_COUNTS[rel]
        for symbol_name, count in counts.items():
            exp = expected.get(symbol_name, 0)
            if count != exp:
                errors.append(
                    f"framework/allocation symbol count mismatch in {rel}: {symbol_name} "
                    f"observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_SYMBOL_COUNTS.items():
        observed = observed_symbols.get(rel, {})
        for symbol_name, exp in expected.items():
            obs = observed.get(symbol_name, 0)
            if obs != exp:
                errors.append(
                    f"framework/allocation symbol count mismatch in {rel}: {symbol_name} "
                    f"observed={obs}, expected={exp}"
                )
        unexpected_symbols = set(observed.keys()) - set(expected.keys())
        if unexpected_symbols:
            errors.append(f"unexpected framework/allocation symbol types in {rel}: {sorted(unexpected_symbols)}")

    for rel, counts in observed_includes.items():
        if rel not in ALLOWED_INCLUDE_COUNTS:
            errors.append(f"forbidden framework includes in unexpected file: {rel} -> {counts}")
            continue
        expected = ALLOWED_INCLUDE_COUNTS[rel]
        for include_name, count in counts.items():
            exp = expected.get(include_name, 0)
            if count != exp:
                errors.append(
                    f"framework include count mismatch in {rel}: {include_name} "
                    f"observed={count}, expected={exp}"
                )

    for rel, expected in ALLOWED_INCLUDE_COUNTS.items():
        observed = observed_includes.get(rel, {})
        for include_name, exp in expected.items():
            obs = observed.get(include_name, 0)
            if obs != exp:
                errors.append(
                    f"framework include count mismatch in {rel}: {include_name} "
                    f"observed={obs}, expected={exp}"
                )
        unexpected_includes = set(observed.keys()) - set(expected.keys())
        if unexpected_includes:
            errors.append(f"unexpected framework include types in {rel}: {sorted(unexpected_includes)}")

    if errors:
        print("Core timing/framework guard FAILED:")
        for err in errors:
            print(f"- {err}")
        return 1

    print("Core timing/framework guard PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())

