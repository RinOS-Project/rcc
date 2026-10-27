#!/usr/bin/env python3
"""Require multiple exact text fragments from one generated file."""

from pathlib import Path
import sys


def main(arguments: list[str]) -> int:
    if len(arguments) < 2:
        print("usage: rcc_check_texts.py FILE EXPECTED_TEXT...", file=sys.stderr)
        return 2

    path = Path(arguments[0])
    expected = arguments[1:]
    try:
        contents = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        print(f"{path}: cannot read input: {error}", file=sys.stderr)
        return 2

    missing = [fragment for fragment in expected if fragment not in contents]
    if missing:
        print(f"{path}: {len(missing)} expected fragment(s) missing:", file=sys.stderr)
        for fragment in missing:
            print(fragment, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
