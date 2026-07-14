#!/usr/bin/env python3
"""Build the documented synthetic 3210 24C128 self-test profile."""

import argparse
from pathlib import Path


SIZE = 0x4000
FLASH_BASE = 0x200000


def build_profile(flash: bytes) -> bytearray:
    image = bytearray([0xFF]) * SIZE

    # Firmware fallback record for NV descriptor 0x0757, variant zero.
    for index in range(0x190):
        source = 0x2D7FDC + index if index < 9 else 0x2D7C08 + index - 9
        raw = (source - FLASH_BASE) ^ 1
        if raw >= len(flash):
            raise ValueError(f"flash is too short for source address 0x{source:08x}")
        image[0x0DB0 + index] = flash[raw]

    patches = {
        # Firmware default written by the contact-service provisioning path at
        # 0x236a78. Startup reloads this four-byte availability record from
        # EEPROM offset 0x0150 through 0x29bb98.
        0x0150: 0x30,
        0x0151: 0x00,
        0x0152: 0x80,
        0x0153: 0x90,
        0x0170: 0x01,
        0x0171: 0x00,
        0x0244: 0x1E,
        0x0245: 0xE1,
        0x011C: 0x00,
        0x011D: 0x00,
        0x011E: 0x1A,
        0x011F: 0xE4,
        0x048C: 0x0A,
        0x048D: 0x00,
        0x048E: 0x0A,
        0x048F: 0x80,
        0x0394: 0x0A,
        0x0395: 0x00,
        0x0396: 0x0A,
        0x0397: 0x80,
        0x0398: 0x09,
        0x0399: 0x00,
        0x039A: 0x00,
        0x039B: 0x00,
    }
    for address, value in patches.items():
        image[address] = value

    # The contact/config block checksum excludes the two correction bytes at
    # 0x0154..0x0155 even though they reside inside the summed range.
    contact_sum = (sum(image[0x0120:0x0244]) - image[0x0154] - image[0x0155]) & 0xFFFF
    image[0x0244] = contact_sum >> 8
    image[0x0245] = contact_sum & 0xFF

    for start, end in ((0x02E0, 0x02EC), (0x0310, 0x0314), (0x0330, 0x0338)):
        image[start:end] = bytes(end - start)

    return image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--flash", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    profile = build_profile(args.flash.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(profile)
    print(f"wrote {args.output} ({len(profile)} bytes)")


if __name__ == "__main__":
    main()
