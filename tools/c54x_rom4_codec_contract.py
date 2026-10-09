#!/usr/bin/env python3
"""Check bounded NSE-1 ROM4 codec-port sequences, not physical port semantics."""

import argparse
from pathlib import Path

if __package__ in (None, ""):
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_port_census import census


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

# Each additional immediate port reader preserves/modifies specific bits and
# writes the result back. These are ROM observations, not register field names.
MASK_SEQUENCES = {
    "port21_3c6f": (0x3c6f, (0x74f8, 0x000b, 0x0021, 0xf340, 0x0005,
                            0xf330, 0xfffd, 0xf495, 0x75f8, 0x000b, 0x0021)),
    "port21_3c84": (0x3c84, (0x74f8, 0x000b, 0x0021, 0xf330, 0xfffb,
                            0xf340, 0x0003, 0xf495, 0x75f8, 0x000b, 0x0021)),
    "port21_4231": (0x4231, (0x74f8, 0x000b, 0x0021, 0xf340, 0x0a00,
                            0xf330, 0xfbff, 0xf495, 0x75f8, 0x000b, 0x0021)),
    "port21_4275": (0x4275, (0x74f8, 0x0008, 0x0021, 0xf030, 0xf7ff,
                            0xf040, 0x0600, 0xf495, 0x75f8, 0x0008, 0x0021)),
    "port21_43c2": (0x43c2, (0x74f8, 0x0008, 0x0021, 0xf040, 0x0140,
                            0xf030, 0xff7f, 0xf495, 0x75f8, 0x0008, 0x0021)),
    "port21_43ef": (0x43ef, (0x74f8, 0x0008, 0x0021, 0xf030, 0xfeff,
                            0xf040, 0x00c0, 0xf495, 0x75f8, 0x0008, 0x0021)),
    "port21_4473": (0x4473, (0x74f8, 0x0008, 0x0021, 0xf040, 0x0200,
                            0xf495, 0x75f8, 0x0008, 0x0021)),
}
SEQUENCES.update(MASK_SEQUENCES)

READ_SITES = [0x321e, 0x33f3, 0x3c6f, 0x3c84, 0x4231,
              0x4275, 0x43c2, 0x43ef, 0x4473]
WRITE_SITES = [0x3225, 0x33fa, 0x3c77, 0x3c8c, 0x4239, 0x427d,
               0x43ca, 0x43f7, 0x4479, 0x4553, 0x4557, 0x455b]


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
    sites = census(image)
    if sites.get(("R", 0x21), []) != READ_SITES:
        raise ValueError("port21 reader census differs from reviewed nine sites")
    if sites.get(("W", 0x21), []) != WRITE_SITES:
        raise ValueError("port21 writer census differs from reviewed twelve sites")
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
    print("Candidate immediate-port coverage: reads=9/9 writes=12/12; uploads excluded.")
    print("Bounded ROM sequences only; physical port ownership remains unresolved.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
