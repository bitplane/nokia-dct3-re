"""Extract a pinned handset staged C54x verifier (not a mask ROM)."""

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
NSE5_FLASH_SHA1 = "53af8324919f455ba8199d2c05f7a921cfb811d5"
NSE5_DESCRIPTOR_OFFSET = 0x2D904
NSE5_PROGRAM_SHA1 = "caca7599d9ca1a7dddf2df37f32be4aacd420deb"


def extract(image, product="8210"):
    profiles = {
        "8210": (FLASH_SHA1, DESCRIPTOR_OFFSET, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "6210": (NPE3_FLASH_SHA1, NPE3_DESCRIPTOR_OFFSET, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "7110": (NSE5_FLASH_SHA1, NSE5_DESCRIPTOR_OFFSET, 210,
                 (0x0f00, 0, 210, 0x0700, 0x00b4, 0), NSE5_PROGRAM_SHA1),
    }
    if product not in profiles:
        raise ValueError("unsupported verifier product")
    expected, offset, words, descriptor, program_sha1 = profiles[product]
    if hashlib.sha1(image).hexdigest() != expected:
        raise ValueError(f"not the pinned {product} flash")
    header = struct.unpack_from(">6H", image, offset)
    if header != descriptor:
        raise ValueError("unexpected verifier descriptor")
    start = offset + 12
    program = image[start:start + words * 2]
    if hashlib.sha1(program).hexdigest() != program_sha1:
        raise ValueError("unexpected staged verifier program")
    return program


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--product", choices=("8210", "6210", "7110"), default="8210")
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
