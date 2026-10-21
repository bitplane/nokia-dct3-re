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
NSM3D_FLASH_SHA1 = "f26c98ffcfffbbd5714889e10cfa41c5f6dd2529"
NSM3D_DESCRIPTOR_OFFSET = 0x1188C0
NSM3D_LOADER_OFFSET = 0x111D14
NSM3D_LOADER_SHA1 = "1250a9e17ce44ec8cc373f222a817f99f505bcdf"
NHM3_FLASH_SHA1 = "95607ce39c383bda75f1e6aeae67a214b787b0a1"
NHM5_FLASH_SHA1 = "d5da65f417595200314eb0115bf46ca1fbf53128"
NHM5_DESCRIPTOR_OFFSET = 0x1091d8
NHM9_FLASH_SHA1 = "3d0dce9c098bdd619c6f3684f739796f64b66169"
NHM9_DESCRIPTOR_OFFSET = 0x1f1e30
NHM9_PROGRAM_SHA1 = "1fc2116ac98a106df41f3822322377a3101aabd5"
NHM9_LOADER_SHA1 = "dc3c2a37913e07a2962d09f6a17cde676a04dd76"
NSB7_FLASH_SHA1 = "4dacc50f57fe295168a53d3e5855a81acf35ab96"


def extract_program_fragment(image, product="8250"):
    profiles = {"8250": (NSM3D_FLASH_SHA1, 0x117700),
                "3350": (NHM9_FLASH_SHA1, 0x1f0c48),
                "8290": (NSB7_FLASH_SHA1, 0x11dca0)}
    if product not in profiles:
        raise ValueError("unsupported fragment product")
    expected, offset = profiles[product]
    if hashlib.sha1(image).hexdigest() != expected:
        raise ValueError(f"not the pinned {product} flash")
    if struct.unpack_from(">6H", image, offset) != (0xff80, 0xff80, 104, 0x200, 0x8c, 0):
        raise ValueError("unexpected bootstrap fragment descriptor")
    fragment = image[offset + 12:offset + 12 + 208]
    if hashlib.sha1(fragment).hexdigest() != "440bf49f1eba4cadb12f7f7581c992b0025807d6":
        raise ValueError("unexpected bootstrap program fragment")
    return fragment


def extract_loader(image, product="8250"):
    """Return the whole owned upload, including its table and code tail."""
    profiles = {"8250": (NSM3D_FLASH_SHA1, NSM3D_LOADER_OFFSET, NSM3D_LOADER_SHA1),
                "3350": (NHM9_FLASH_SHA1, 0x1ead0c, NHM9_LOADER_SHA1),
                "8290": (NSB7_FLASH_SHA1, 0x1183a4, '70c931c40b3e7cdebc8d09f2488e3cc1e3c848d9')}
    if product not in profiles:
        raise ValueError("unsupported loader product")
    expected, offset, program_sha1 = profiles[product]
    if hashlib.sha1(image).hexdigest() != expected:
        raise ValueError(f"not the pinned {product} flash")
    if struct.unpack_from(">6H", image, offset) != (
            0xfd00, 0xff80, 0x027e, 0x0500, 0x0078, 0):
        raise ValueError("unexpected loader descriptor")
    start = offset + 12
    payload = image[start:start + 638 * 2]
    if hashlib.sha1(payload).hexdigest() != program_sha1:
        raise ValueError("unexpected loader upload")
    return payload


def extract(image, product="8210"):
    profiles = {
        "8210": (FLASH_SHA1, DESCRIPTOR_OFFSET, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "6210": (NPE3_FLASH_SHA1, NPE3_DESCRIPTOR_OFFSET, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "7110": (NSE5_FLASH_SHA1, NSE5_DESCRIPTOR_OFFSET, 210,
                 (0x0f00, 0, 210, 0x0700, 0x00b4, 0), NSE5_PROGRAM_SHA1),
        "8250": (NSM3D_FLASH_SHA1, NSM3D_DESCRIPTOR_OFFSET, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "6250": (NHM3_FLASH_SHA1, 0x1e544, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "3310": (NHM5_FLASH_SHA1, NHM5_DESCRIPTOR_OFFSET, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
        "3350": (NHM9_FLASH_SHA1, NHM9_DESCRIPTOR_OFFSET, 247,
                 (0x0f00, 0, 247, 0x0f00, 0x00dc, 0), NHM9_PROGRAM_SHA1),
        "8290": (NSB7_FLASH_SHA1, 0x11ee88, PROGRAM_WORDS,
                 (0x0f00, 0, PROGRAM_WORDS, 0x0f00, 0x00dc, 0), PROGRAM_SHA1),
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
    parser.add_argument("--product", choices=("8210", "6210", "7110", "8250", "6250", "3310", "3350", "8290"), default="8210")
    kind = parser.add_mutually_exclusive_group()
    kind.add_argument("--loader", action="store_true", help="extract the supported product's second-stage upload")
    kind.add_argument("--fragment", action="store_true", help="extract the supported product's owned bootstrap fragment")
    args = parser.parse_args()
    try:
        if args.output.resolve() == args.flash.resolve():
            raise ValueError("output must not overwrite the source flash")
        image = args.flash.read_bytes()
        program = (extract_loader(image, args.product) if args.loader else
                   extract_program_fragment(image, args.product) if args.fragment else
                   extract(image, args.product))
    except (OSError, ValueError) as error:
        parser.exit(1, f"NSM-3 verifier extraction failed: {error}\n")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(program)
    kind = "loader upload" if args.loader else "bootstrap fragment" if args.fragment else "verifier"
    print(f"{args.product} {kind}: {len(program) // 2} words, SHA-1 {hashlib.sha1(program).hexdigest()}")


if __name__ == "__main__":
    main()
