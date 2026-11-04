#!/usr/bin/env python3
"""Check the SSE2 MMX-compatibility opcodes emitted by RCC."""

import argparse
import re
import subprocess
import sys


EXPECTED = {
    "_mm_movepi64_pi64": bytes((0xF2, 0x0F, 0xD6)),
    "_mm_movpi64_epi64": bytes((0xF3, 0x0F, 0xD6)),
    "_mm_mul_su32": bytes((0x0F, 0xF4)),
    "_mm_cvtpd_pi32": bytes((0x66, 0x0F, 0x2D)),
    "_mm_cvttpd_pi32": bytes((0x66, 0x0F, 0x2C)),
    "_mm_cvtpi32_pd": bytes((0x66, 0x0F, 0x2A)),
    "_mm_maskmoveu_si128": bytes((0x66, 0x0F, 0xF7)),
}


def symbol_bytes(objdump, object_path, symbol, arch):
    result = subprocess.run(
        [objdump, "-d", "-M", "att", f"--disassemble={symbol}", object_path],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(f"objdump failed for {arch} {symbol}")

    encoded = bytearray()
    for line in result.stdout.splitlines():
        match = re.match(r"\s*[0-9a-fA-F]+:\s*(.*)$", line)
        if not match:
            continue
        for token in match.group(1).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            encoded.append(int(token, 16))
    return bytes(encoded), result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--arch", choices=("i686", "x86_64"), required=True)
    parser.add_argument("object")
    args = parser.parse_args()

    for symbol, opcode in EXPECTED.items():
        encoded, output = symbol_bytes(
            args.objdump, args.object, symbol, args.arch
        )
        if opcode not in encoded:
            sys.stderr.write(output)
            raise SystemExit(
                f"{args.arch} {symbol} does not contain opcode "
                + " ".join(f"{byte:02X}" for byte in opcode)
            )
        print(
            f"verified {args.arch} {symbol}: "
            + " ".join(f"{byte:02X}" for byte in opcode)
        )


if __name__ == "__main__":
    main()
