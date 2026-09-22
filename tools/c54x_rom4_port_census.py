#!/usr/bin/env python3
"""List candidate C54x PORTR/PORTW sites in a big-endian word image."""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path


def census(image: bytes) -> dict[tuple[str, int], list[int]]:
    if len(image) % 2:
        raise ValueError("C54x image must contain complete 16-bit words")
    words = [int.from_bytes(image[i:i + 2], "big") for i in range(0, len(image), 2)]
    sites: dict[tuple[str, int], list[int]] = defaultdict(list)
    for address, opcode in enumerate(words):
        family = opcode & 0xff00
        if family not in (0x7400, 0x7500):
            continue
        # Absolute Smem (F8) consumes an address extension before the port.
        port_index = address + (2 if opcode & 0xff == 0xf8 else 1)
        if port_index < len(words) and words[port_index] < 0x100:
            direction = "R" if family == 0x7400 else "W"
            sites[(direction, words[port_index])].append(address)
    return dict(sites)


def direct_callers(image: bytes, target: int) -> list[tuple[int, bool]]:
    """Candidate CALL/CALLD sites; raw words need runtime confirmation."""
    if len(image) % 2:
        raise ValueError("C54x image must contain complete 16-bit words")
    words = [int.from_bytes(image[i:i + 2], "big") for i in range(0, len(image), 2)]
    return [(address, opcode == 0xf274)
            for address, opcode in enumerate(words[:-1])
            if opcode in (0xf074, 0xf274) and words[address + 1] == target]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--port", type=lambda value: int(value, 0))
    parser.add_argument("--call-target", type=lambda value: int(value, 0))
    args = parser.parse_args()
    image = args.image.read_bytes()
    if args.call_target is not None:
        for address, delayed in direct_callers(image, args.call_target):
            print(f"{address:04x} {'CALLD' if delayed else 'CALL'} {args.call_target:04x}")
        return
    for (direction, port), addresses in sorted(census(image).items()):
        if args.port is None or args.port == port:
            print(f"{direction} {port:02x} {len(addresses):3d} " +
                  " ".join(f"{address:04x}" for address in addresses))


if __name__ == "__main__":
    main()
