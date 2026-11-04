#!/usr/bin/env python3
"""Check conversion builtin encodings in named i686 and x86_64 functions."""

import argparse
import re
import subprocess
import sys


EXPECTED = {
    "i686": {
        "rcc_cvtpd2ps": "66 0f 5a",
        "rcc_cvtpd2dq": "f2 0f e6",
        "rcc_cvttpd2dq": "66 0f e6",
        "rcc_cvtps2pd": "0f 5a",
        "rcc_cvtdq2pd": "f3 0f e6",
        "rcc_cvtdq2ps": "0f 5b",
        "rcc_cvtps2dq": "66 0f 5b",
        "rcc_cvttps2dq": "f3 0f 5b",
        "rcc_cvtsd2ss": "f2 0f 5a",
        "rcc_cvtss2sd": "f3 0f 5a",
        "rcc_cvtsi32sd": "f2 0f 2a",
        "rcc_cvtsd2si": "f2 0f 2d",
        "rcc_cvttsd2si": "f2 0f 2c",
    },
    "x86_64": {
        "rcc_cvtpd2ps": "66 0f 5a",
        "rcc_cvtpd2dq": "f2 0f e6",
        "rcc_cvttpd2dq": "66 0f e6",
        "rcc_cvtps2pd": "0f 5a",
        "rcc_cvtdq2pd": "f3 0f e6",
        "rcc_cvtdq2ps": "0f 5b",
        "rcc_cvtps2dq": "66 0f 5b",
        "rcc_cvttps2dq": "f3 0f 5b",
        "rcc_cvtsd2ss": "f2 0f 5a",
        "rcc_cvtss2sd": "f3 0f 5a",
        "rcc_cvtsi32sd": "f2 0f 2a",
        "rcc_cvtsi64sd": "f2 48 0f 2a",
        "rcc_cvtsd2si": "f2 0f 2d",
        "rcc_cvttsd2si": "f2 0f 2c",
        "rcc_cvtsd2si64": "f2 48 0f 2d",
        "rcc_cvttsd2si64": "f2 48 0f 2c",
    },
}


def decode_function_bytes(disassembly):
    for line in disassembly.splitlines():
        match = re.match(r"\s*[0-9a-fA-F]+:\s*(.*)$", line)
        if not match:
            continue
        encoded = []
        for token in match.group(1).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            encoded.append(int(token, 16))
        if encoded:
            yield bytes(encoded)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--arch", choices=EXPECTED, required=True)
    parser.add_argument("object")
    args = parser.parse_args()

    for symbol, expected_hex in EXPECTED[args.arch].items():
        result = subprocess.run(
            [args.objdump, "-d", "-M", "att", f"--disassemble={symbol}",
             args.object],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode != 0:
            sys.stderr.write(result.stderr)
            raise SystemExit(
                f"objdump failed for {args.arch} symbol {symbol}"
            )
        expected = bytes.fromhex(expected_hex)
        if not any(expected in instruction
                   for instruction in decode_function_bytes(result.stdout)):
            sys.stderr.write(result.stdout)
            raise SystemExit(
                f"{args.arch} {symbol} does not contain opcode "
                f"{expected_hex}"
            )

    print(f"verified {len(EXPECTED[args.arch])} {args.arch} conversion opcodes")


if __name__ == "__main__":
    main()
