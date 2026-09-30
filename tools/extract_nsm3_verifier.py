"""Extract the stock NSM-3 externally staged C54x verifier (not a mask ROM)."""

import argparse
import hashlib
from pathlib import Path
import struct


FLASH_SHA1 = "c1a0fe95cedb89a92b19654208cc4855e1a4988e"
DESCRIPTOR_OFFSET = 0x11BCF0
PROGRAM_WORDS = 223
PROGRAM_SHA1 = "6646da3c5be9c70deda7e0b5b9f257d5d2ace815"


def extract(image):
    if hashlib.sha1(image).hexdigest() != FLASH_SHA1:
        raise ValueError("not the pinned NSM-3 v5.31 PPM-C flash")
    header = struct.unpack_from(">6H", image, DESCRIPTOR_OFFSET)
    if header != (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0):
        raise ValueError("unexpected verifier descriptor")
    start = DESCRIPTOR_OFFSET + 12
    program = image[start:start + PROGRAM_WORDS * 2]
    if hashlib.sha1(program).hexdigest() != PROGRAM_SHA1:
        raise ValueError("unexpected staged verifier program")
    return program


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        program = extract(args.flash.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"NSM-3 verifier extraction failed: {error}\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(program)
    print(f"NSM-3 verifier: {len(program) // 2} words at 0x0f00, SHA-1 {hashlib.sha1(program).hexdigest()}")


if __name__ == "__main__":
    main()
