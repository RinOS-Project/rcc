#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Structural signer double used to test argv handling and atomic publication."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import struct
import tempfile


RIN_MAGIC = 0x004E4952
NDRV_MAGIC = 0x5652444E
RDS1_MAGIC = 0x31534452


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path, required=True)
    parser.add_argument("--key", type=Path, required=True)
    parser.add_argument("--public-key", type=Path, required=True)
    args = parser.parse_args()

    # Git may materialize these text-only fixture keys with CRLF on a Windows
    # checkout even when the signer is executed through WSL. Keep the test
    # double deterministic across both host layouts.
    key = args.key.read_bytes().replace(b"\r\n", b"\n")
    public_key = args.public_key.read_bytes().replace(b"\r\n", b"\n")
    if key == b"FAIL\n":
        return 23
    if key == b"INVALID\n":
        args.output.write_bytes(args.input.read_bytes())
        return 0
    if key != b"RCC TEST PRIVATE KEY\n" or public_key != b"RCC TEST PUBLIC KEY\n":
        return 24

    image = bytearray(args.input.read_bytes())
    if len(image) < 256:
        return 25
    magic = struct.unpack_from("<I", image)[0]
    if magic == RIN_MAGIC:
        flags_offset, signed_flag = 16, 0x40
        signature_fields, hash_offset = 100, 116
    elif magic == NDRV_MAGIC:
        flags_offset, signed_flag = 20, 0x01
        signature_fields, hash_offset = 128, 144
    else:
        return 26

    signature = hashlib.sha256(key + public_key + image).digest() * 8
    signature_offset = len(image)
    signature_size = 48 + len(signature)
    flags = struct.unpack_from("<I", image, flags_offset)[0] | signed_flag
    struct.pack_into("<I", image, flags_offset, flags)
    struct.pack_into(
        "<QIHH", image, signature_fields, signature_offset, signature_size, 1, 1
    )
    image[hash_offset : hash_offset + 32] = hashlib.sha256(
        image[256:signature_offset]
    ).digest()
    envelope = struct.pack(
        "<IHHHH32sI",
        RDS1_MAGIC,
        1,
        48,
        1,
        len(signature),
        hashlib.sha256(public_key).digest(),
        0,
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="wb", prefix=f".{args.output.name}.", suffix=".tmp",
        dir=args.output.parent, delete=False
    ) as handle:
        temporary = Path(handle.name)
        handle.write(image)
        handle.write(envelope)
        handle.write(signature)
        handle.flush()
        os.fsync(handle.fileno())
    os.replace(temporary, args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
