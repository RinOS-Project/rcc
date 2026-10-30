#!/usr/bin/env python3
"""Require compiler outputs to use the same mangled symbol for a fixture."""

import re
import sys
from pathlib import Path


def symbols(path, marker):
    assembly = Path(path).read_text(encoding="utf-8")
    names = re.findall(
        r"^\s*\.(?:globl|global|weak)\s+([^\s,]+)", assembly, re.MULTILINE
    )
    return [name for name in names if marker in name]


def main():
    if len(sys.argv) < 4:
        raise SystemExit(
            "usage: compare_mangled_symbols.py MARKER REFERENCE CANDIDATE..."
        )
    marker, reference_path, *candidate_paths = sys.argv[1:]
    reference = symbols(reference_path, marker)
    if len(reference) != 1:
        raise SystemExit(
            f"expected one {marker!r} symbol in {reference_path}, found {reference}"
        )
    for path in candidate_paths:
        candidate = symbols(path, marker)
        if candidate != reference:
            raise SystemExit(
                f"mangled symbol differs in {path}: expected {reference}, got {candidate}"
            )
    print(f"matching {marker} symbol: {reference[0]}")


if __name__ == "__main__":
    main()
