#!/usr/bin/env python3
"""Check MOVQ xmm,xmm in the RCC-generated intrinsic wrapper."""

import argparse
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--arch", choices=("i686", "x86_64"), required=True)
    parser.add_argument("object")
    args = parser.parse_args()

    result = subprocess.run(
        [args.objdump, "-d", "-M", "att", "--disassemble=_mm_move_epi64",
         args.object],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"objdump failed for {args.arch} _mm_move_epi64")

    for line in result.stdout.splitlines():
        match = re.match(r"\s*[0-9a-fA-F]+:\s*(.*)$", line)
        if not match:
            continue
        encoded = []
        for token in match.group(1).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            encoded.append(int(token, 16))
        if bytes((0xF3, 0x0F, 0x7E)) in bytes(encoded):
            print(f"verified {args.arch} MOVQ xmm,xmm opcode F3 0F 7E")
            return

    sys.stderr.write(result.stdout)
    raise SystemExit(
        f"{args.arch} _mm_move_epi64 does not contain opcode F3 0F 7E"
    )


if __name__ == "__main__":
    main()
