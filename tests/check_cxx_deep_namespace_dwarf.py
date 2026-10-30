#!/usr/bin/env python3
"""Check that RCC's DWARF keeps a deeply qualified template-template name."""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} object.ro", file=sys.stderr)
        return 2

    qualified_name = "::".join(
        f"namespace_component_{index:03d}" for index in range(34)
    ) + "::DeepTemplate"
    object_bytes = Path(sys.argv[1]).read_bytes()
    if qualified_name.encode("ascii") not in object_bytes:
        print(
            "DWARF is missing the complete deeply qualified template name",
            file=sys.stderr,
        )
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
