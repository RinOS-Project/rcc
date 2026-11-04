#!/usr/bin/env python3
"""Check 64-bit MOVNTI in the RCC-generated intrinsic wrapper."""

import argparse
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", required=True)
    parser.add_argument("object")
    args = parser.parse_args()

    result = subprocess.run(
        [args.objdump, "-d", "-M", "att", "--disassemble=_mm_stream_si64",
         args.object],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit("objdump failed for _mm_stream_si64")

    for line in result.stdout.splitlines():
        match = re.match(r"\s*[0-9a-fA-F]+:\s*(.*)$", line)
        if not match:
            continue
        encoded = []
        for token in match.group(1).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            encoded.append(int(token, 16))
        if bytes((0x48, 0x0F, 0xC3)) in bytes(encoded):
            print("verified x86_64 MOVNTI m64,r64 opcode 48 0F C3")
            return

    sys.stderr.write(result.stdout)
    raise SystemExit("_mm_stream_si64 does not contain opcode 48 0F C3")


if __name__ == "__main__":
    main()
