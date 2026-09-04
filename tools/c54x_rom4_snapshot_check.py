#!/usr/bin/env python3
"""Validate a private ROM4 transform-entry snapshot without distributing it."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import struct
import sys


EXPECTED_HASHES = {
    "prog": "6ade0d4deb00b0ede99934fab25d57d7cb2589ea2a147accce0f3a11346d09a2",
    "api": "0886b3e9cf1547517c8d44cec9060eb765e515437485937df691106f549bb2ba",
    "data": "4d3af5ada1ca1d60904550b3d9a10a8b6cb8698377f6dbf2424f5cf7a459db3b",
    "regs": "59978e19dc33d1bf99cb134853a4a63da8b8741e96fb4eef88d30b5e4869a39e",
}

EXPECTED_REGISTERS = {
    "pc": 0x4B73, "sp": 0x1EC3, "st0": 0x201F, "st1": 0x2103,
    "pmst": 0xFFAC, "imr": 0x53FF, "ifr": 0x0000,
    "ar2": 0x0825, "ar3": 0x06E3, "ar4": 0x001A,
    "ar5": 0x12CA, "ar6": 0x06E3,
}

ENTRY_RESPONSE = (
    0x1618, 0x4905, 0x4410, 0x9019, 0x87FF, 0x0000,
    0x4905, 0x4410, 0x9019, 0x87FF, 0x0000,
)


def words(path: pathlib.Path, address: int, count: int, base: int = 0x0800):
    data = path.read_bytes()
    offset = (address - base) * 2
    if offset < 0 or offset + count * 2 > len(data):
        raise ValueError(f"address {address:04x} is outside {path.name}")
    return struct.unpack_from(f">{count}H", data, offset)


def parse_registers(text: str) -> dict[str, int]:
    result = {}
    for name, value in re.findall(r"\b([a-z][a-z0-9]*)=0x([0-9a-fA-F]+)", text):
        result[name] = int(value, 16)
    return result


def check(prefix: pathlib.Path, require_hashes: bool = True) -> dict[str, object]:
    paths = {suffix: pathlib.Path(f"{prefix}.{suffix}") for suffix in EXPECTED_HASHES}
    for path in paths.values():
        if not path.is_file():
            raise ValueError(f"missing snapshot component {path}")
    hashes = {name: hashlib.sha256(path.read_bytes()).hexdigest()
              for name, path in paths.items()}
    if require_hashes:
        wrong = [name for name, digest in hashes.items()
                 if digest != EXPECTED_HASHES[name]]
        if wrong:
            raise ValueError("snapshot digest mismatch: " + ", ".join(wrong))
    registers = parse_registers(paths["regs"].read_text())
    wrong_regs = [name for name, value in EXPECTED_REGISTERS.items()
                  if registers.get(name) != value]
    if wrong_regs:
        raise ValueError("entry register mismatch: " + ", ".join(wrong_regs))
    response = words(paths["api"], 0x1200, len(ENTRY_RESPONSE))
    if response != ENTRY_RESPONSE:
        raise ValueError("entry response buffer is not the raw identity object")
    return {"hashes": hashes, "registers": registers, "response_words": len(response)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("prefix", type=pathlib.Path)
    parser.add_argument("--allow-different-hash", action="store_true")
    args = parser.parse_args()
    try:
        result = check(args.prefix, not args.allow_different_hash)
    except (OSError, ValueError) as error:
        print(f"ROM4 C54x snapshot rejected: {error}", file=sys.stderr)
        return 1
    print(
        "ROM4 C54x entry snapshot: "
        f"pc={result['registers']['pc']:04x} "
        f"response_words={result['response_words']} components=4"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
