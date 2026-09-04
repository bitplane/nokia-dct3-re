#!/usr/bin/env python3
"""Materialize a private ROM4 C54x cold-reset data image from the DROM listing."""

from __future__ import annotations

import argparse
import pathlib
import re
import struct


LINE = re.compile(r"^\s*([0-9a-fA-F]{4})\s*:\s*([0-9a-fA-F]{4})\s*$")
FIRST = 0xB000
LAST = 0xEFFF


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    words: dict[int, int] = {}
    for number, raw in enumerate(args.source.read_text().splitlines(), 1):
        match = LINE.fullmatch(raw)
        if not match:
            raise ValueError(f"unsupported DROM line {number}: {raw!r}")
        address, value = (int(item, 16) for item in match.groups())
        if not FIRST <= address <= LAST:
            raise ValueError(f"DROM address 0x{address:04x} is outside B000:EFFF")
        if address in words:
            raise ValueError(f"duplicate DROM address 0x{address:04x}")
        words[address] = value

    expected = LAST - FIRST + 1
    if len(words) != expected:
        raise ValueError(f"DROM contains {len(words)} words; expected {expected}")

    image = bytearray(0x10000 * 2)
    for address, value in words.items():
        struct.pack_into(">H", image, address * 2, value)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(image)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
