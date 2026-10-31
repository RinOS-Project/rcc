#!/usr/bin/env python3
"""Check emitted machine-code bytes for one function in RCC .s output."""

import re
import sys
from pathlib import Path


def function_marker(source: str, name: str) -> re.Match[str] | None:
    marker = re.compile(
        rf"(?m)^\.globl {re.escape(name)}\r?\n"
        rf"{re.escape(name)}:\r?$"
    )
    return marker.search(source)


def main() -> int:
    if len(sys.argv) != 5:
        print(
            "usage: check_asm_function_size.py ASM START_SYMBOL "
            "END_SYMBOL MAX_BYTES",
            file=sys.stderr,
        )
        return 2

    assembly_path = Path(sys.argv[1])
    start_name, end_name = sys.argv[2], sys.argv[3]
    try:
        limit = int(sys.argv[4], 10)
    except ValueError:
        print(f"invalid byte limit: {sys.argv[4]}", file=sys.stderr)
        return 2
    if limit <= 0:
        print(f"byte limit must be positive: {limit}", file=sys.stderr)
        return 2

    source = assembly_path.read_text(encoding="utf-8")
    start = function_marker(source, start_name)
    end = function_marker(source, end_name)
    if start is None or end is None or start.end() >= end.start():
        print(
            f"could not find ordered function markers {start_name}, {end_name} "
            f"in {assembly_path}",
            file=sys.stderr,
        )
        return 2

    body = source[start.end() : end.start()]
    byte_lines = re.findall(r"(?m)^\s*\.byte\s+([^\r\n]+)", body)
    if not byte_lines:
        print(f"no emitted code bytes for {start_name}", file=sys.stderr)
        return 2

    byte_count = 0
    for line in byte_lines:
        for token in line.split(","):
            if re.fullmatch(r"\s*0x[0-9a-fA-F]{1,2}\s*", token) is None:
                print(f"invalid .byte entry in {start_name}: {token}", file=sys.stderr)
                return 2
            byte_count += 1

    print(f"{start_name}: {byte_count} emitted bytes (limit {limit})")
    if byte_count > limit:
        print(
            f"{start_name} exceeds the emitted-code limit of {limit} bytes",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
