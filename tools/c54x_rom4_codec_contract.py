#!/usr/bin/env python3
"""Check bounded NSE-1 ROM4 codec-port sequences, not physical port semantics."""

import argparse
from pathlib import Path


SEQUENCES = {
    "frame_entry_port21_set": (0x321e, (
        0x74f8, 0x0008, 0x0021,  # PORTR 21, AL
        0xf040, 0x0c00,          # OR #0c00, A
        0x7212, 0x00aa,          # MVDM aa, AR2
        0x75f8, 0x0008, 0x0021,  # PORTW AL, 21
    )),
    "frame_exit_port21_clear": (0x33f3, (
        0x74f8, 0x0008, 0x0021,  # PORTR 21, AL
        0xf030, 0xf7ff,          # AND #f7ff, A
        0x8a19, 0x8a1c,          # Restore saved registers; not port writes.
        0x75f8, 0x0008, 0x0021,  # PORTW AL, 21
    )),
    "port21_initialization": (0x454e, (
        0x7692, 0x0482,          # ST #0482, *AR2+
        0x7682, 0x1482,          # ST #1482, *AR2
        0xf495, 0x7582, 0x0021,  # NOP; PORTW *AR2, 21
        0xf495, 0xf495,
        0x758a, 0x0021,          # PORTW *AR2-, 21
        0xf495, 0xf495,
        0x7582, 0x0021,          # PORTW *AR2, 21
    )),
}


def check(image: bytes) -> list[str]:
    if len(image) % 2:
        raise ValueError("C54x image must contain complete big-endian words")
    results = []
    for name, (address, expected) in SEQUENCES.items():
        raw = image[address * 2:(address + len(expected)) * 2]
        words = tuple(int.from_bytes(raw[i:i + 2], "big")
                      for i in range(0, len(raw), 2))
        if words != expected:
            raise ValueError(f"{name}: ROM words differ at {address:04x}")
        results.append(name)
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    try:
        results = check(args.image.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: " + ", ".join(results))
    print("Bounded ROM sequences only; physical port ownership remains unresolved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
