#!/usr/bin/env python3
"""Build a bounded NAM-2 v5.84 product-state research fixture."""

import argparse
from pathlib import Path


EEPROM_MAGIC = b"EEPROM"
FLASH_SIZE = 0x200000
PMM_OFFSET = 0x1F0000
LOGICAL_DATA_OFFSET = 0x26
IDENTITY_BLOCK_SIZE = 0x120
IDENTITY_CHECKSUM_OFFSET = 0x11C
VERSION_REFERENCE_OFFSET = 0x270


def make_profile(base: bytes, donor: bytes) -> bytes:
    if len(base) != PMM_OFFSET:
        raise ValueError(f"expected a {PMM_OFFSET:#x}-byte MCU/PPM base")
    if len(donor) != FLASH_SIZE:
        raise ValueError(f"expected a {FLASH_SIZE:#x}-byte full-flash donor")

    # Preserve the selected firmware image.  Only the physical PMM partition is
    # borrowed from v5.21; copying the donor wholesale would silently replace
    # the v5.84 executable when MAME restores the persistent flash image.
    result = bytearray(base + donor[PMM_OFFSET:])
    logical = None
    search = PMM_OFFSET
    while True:
        magic = result.find(EEPROM_MAGIC, search)
        if magic < 0:
            break
        candidate = magic - 6 + LOGICAL_DATA_OFFSET
        if (magic >= PMM_OFFSET + 6
                and candidate + VERSION_REFERENCE_OFFSET + 2 <= len(result)
                and result[candidate + VERSION_REFERENCE_OFFSET:candidate + VERSION_REFERENCE_OFFSET + 2]
                == bytes.fromhex("933d")):
            logical = candidate
            break
        search = magic + 1
    if logical is None:
        raise ValueError("expected v5.21 EEPROM catalog reference 0x933d is absent")

    checksum = sum(result[logical:logical + IDENTITY_CHECKSUM_OFFSET]) & 0xffffffff
    result[logical + IDENTITY_CHECKSUM_OFFSET:logical + IDENTITY_BLOCK_SIZE] = (
        checksum.to_bytes(4, "big"))
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("base", type=Path)
    parser.add_argument("donor", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    profile = make_profile(args.base.read_bytes(), args.donor.read_bytes())
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
