#!/usr/bin/env python3
"""Build a bounded NAM-2 v5.84 product-state research fixture."""

import argparse
from pathlib import Path


EEPROM_MAGIC = b"EEPROM"
LOGICAL_DATA_OFFSET = 0x26
IDENTITY_BLOCK_SIZE = 0x120
IDENTITY_CHECKSUM_OFFSET = 0x11C
VERSION_REFERENCE_OFFSET = 0x270


def make_profile(donor: bytes) -> bytes:
    magic = donor.find(EEPROM_MAGIC)
    if magic < 6:
        raise ValueError("EEPROM catalog missing")
    catalog = magic - 6
    logical = catalog + LOGICAL_DATA_OFFSET
    if logical + VERSION_REFERENCE_OFFSET + 2 > len(donor):
        raise ValueError("EEPROM logical data exceeds image")
    if donor[logical + VERSION_REFERENCE_OFFSET:logical + VERSION_REFERENCE_OFFSET + 2] != bytes.fromhex("933d"):
        raise ValueError("expected v5.21 reference 0x933d is absent")

    result = bytearray(donor)
    checksum = sum(result[logical:logical + IDENTITY_CHECKSUM_OFFSET]) & 0xffffffff
    result[logical + IDENTITY_CHECKSUM_OFFSET:logical + IDENTITY_BLOCK_SIZE] = (
        checksum.to_bytes(4, "big"))
    return bytes(result)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("donor", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    profile = make_profile(args.donor.read_bytes())
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
