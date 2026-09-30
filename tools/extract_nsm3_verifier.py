"""Extract a pinned NSM-3/NPE-3 staged C54x verifier (not a mask ROM)."""

import argparse
import hashlib
from pathlib import Path
import struct


FLASH_SHA1 = "c1a0fe95cedb89a92b19654208cc4855e1a4988e"
DESCRIPTOR_OFFSET = 0x11BCF0
PROGRAM_WORDS = 223
PROGRAM_SHA1 = "6646da3c5be9c70deda7e0b5b9f257d5d2ace815"
NPE3_FLASH_SHA1 = "3d9ea319503e78ec69b60d72cda23e461e118ea9"
NPE3_DESCRIPTOR_OFFSET = 0x25c2c


def extract(image, product="8210"):
    expected, offset = ((FLASH_SHA1, DESCRIPTOR_OFFSET) if product == "8210"
                        else (NPE3_FLASH_SHA1, NPE3_DESCRIPTOR_OFFSET))
    if product not in ("8210", "6210"):
        raise ValueError("unsupported verifier product")
    if hashlib.sha1(image).hexdigest() != expected:
        raise ValueError(f"not the pinned {product} flash")
    header = struct.unpack_from(">6H", image, offset)
    if header != (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0):
        raise ValueError("unexpected verifier descriptor")
    start = offset + 12
    program = image[start:start + PROGRAM_WORDS * 2]
    if hashlib.sha1(program).hexdigest() != PROGRAM_SHA1:
        raise ValueError("unexpected staged verifier program")
    return program


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--product", choices=("8210", "6210"), default="8210")
    args = parser.parse_args()
    try:
        program = extract(args.flash.read_bytes(), args.product)
    except (OSError, ValueError) as error:
        parser.exit(1, f"NSM-3 verifier extraction failed: {error}\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(program)
    print(f"{args.product} verifier: {len(program) // 2} words at 0x0f00, SHA-1 {hashlib.sha1(program).hexdigest()}")


if __name__ == "__main__":
    main()
