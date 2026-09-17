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
    logical = None
    search = 0
    while True:
        magic = donor.find(EEPROM_MAGIC, search)
        if magic < 0:
            break
        candidate = magic - 6 + LOGICAL_DATA_OFFSET
        if (magic >= 6 and candidate + VERSION_REFERENCE_OFFSET + 2 <= len(donor)
                and donor[candidate + VERSION_REFERENCE_OFFSET:candidate + VERSION_REFERENCE_OFFSET + 2]
                == bytes.fromhex("933d")):
            logical = candidate
            break
        search = magic + 1
    if logical is None:
        raise ValueError("expected v5.21 EEPROM catalog reference 0x933d is absent")

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
