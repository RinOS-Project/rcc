#!/usr/bin/env python3
"""Generate a DWARF fixture with more than 255 source-file entries."""

from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: generate_debug_info_large_file_table.py OUTPUT")
    output = Path(sys.argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="\n") as source:
        for index in range(260):
            source.write(f'#line 1 "debug_info_file_{index:03}.c"\n')
            if index == 259:
                source.write(
                    "int debug_info_large_file_259(void) {\n"
                    "    {\n"
                    "        volatile int debug_info_large_file_local = 259;\n"
                    "        return debug_info_large_file_local;\n"
                    "    }\n"
                    "}\n"
                )
            else:
                source.write(
                    f"int debug_info_large_file_{index}(void) "
                    f"{{ return {index}; }}\n"
                )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
