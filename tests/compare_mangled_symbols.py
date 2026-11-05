#!/usr/bin/env python3
"""Require compiler outputs to use the same mangled symbol for a fixture."""

import re
import sys
from pathlib import Path


def symbols(path, marker, all_symbols=False):
    assembly = Path(path).read_text(encoding="utf-8")
    names = re.findall(
        r"^\s*\.(?:globl|global|weak)\s+([^\s,]+)", assembly, re.MULTILINE
    )
    matches = [name for name in names if marker in name]
    if all_symbols:
        return sorted({name for name in matches if name.startswith("_Z")})
    return matches


def main():
    arguments = sys.argv[1:]
    all_symbols = bool(arguments and arguments[0] == "--all")
    if all_symbols:
        arguments = arguments[1:]
    if len(arguments) < 3:
        raise SystemExit(
            "usage: compare_mangled_symbols.py [--all] MARKER REFERENCE CANDIDATE..."
        )
    marker, reference_path, *candidate_paths = arguments
    reference = symbols(reference_path, marker, all_symbols)
    if not reference or (not all_symbols and len(reference) != 1):
        raise SystemExit(
            f"expected {'at least one' if all_symbols else 'one'} {marker!r} symbol "
            f"in {reference_path}, found {reference}"
        )
    for path in candidate_paths:
        candidate = symbols(path, marker, all_symbols)
        if candidate != reference:
            raise SystemExit(
                f"mangled symbol differs in {path}: expected {reference}, got {candidate}"
            )
    if all_symbols:
        print(f"matching {len(reference)} {marker} mangled symbols")
    else:
        print(f"matching {marker} symbol: {reference[0]}")


if __name__ == "__main__":
    main()
