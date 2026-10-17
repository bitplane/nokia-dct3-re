#!/usr/bin/env python3
"""Bound acquired ROM4 EFR descriptor coverage; not codec execution proof."""

import argparse
from collections import Counter
from pathlib import Path
import struct


# GSM 06.60 parameter widths; GSM 05.03/45.003 section 3.1.1 CRC inputs.
WIDTHS = (7, 8, 9, 8, 6) + tuple(
    width for pitch in (9, 6, 9, 6)
    for width in (pitch, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 5))
CRC_INPUTS = (
    39, 40, 41, 42, 43, 44, 48, 87, 45, 2,
    3, 8, 10, 18, 19, 24, 46, 47, 142, 143,
    144, 145, 146, 147, 92, 93, 195, 196, 98, 137,
    148, 94, 197, 149, 150, 95, 198, 4, 5, 11,
    12, 16, 9, 6, 7, 13, 17, 20, 96, 199,
    1, 14, 15, 21, 25, 26, 28, 151, 201, 190,
    240, 88, 138, 191, 241)


def speech_descriptors():
    """MSB-first parameters, using TI BITT's reversed bit numbering."""
    return tuple((index << 8) | (15 - bit)
                 for index, width in enumerate(WIDTHS)
                 for bit in range(width - 1, -1, -1))


def expected_coverage():
    speech = speech_descriptors()
    expected = Counter(speech)
    for position in (70, 120, 173, 223):
        expected[speech[position - 1]] += 2
    expected.update((57 << 8) | (15 - bit) for bit in range(7, -1, -1))
    expected[0x3907] += 12  # CRC bit 8: zero only if the CRC is eight bits.
    return expected


def expected_crc_selectors():
    speech = speech_descriptors()
    selectors = tuple(speech[position - 1] for position in CRC_INPUTS)
    # The routine seeds its register from eight bits before shifting LSB-first.
    return selectors[:8][::-1] + selectors[8:]


def check(image):
    def words(address, count):
        if len(image) < (address + count) * 2:
            raise ValueError("truncated big-endian DSP data image")
        return struct.unpack_from(f">{count}H", image, address * 2)

    if Counter(words(0xd8c2, 272)) != expected_coverage():
        raise ValueError("EFR packing descriptor coverage differs")
    if words(0xd9d2, 65) != expected_crc_selectors():
        raise ValueError("EFR CRC selector order differs")
    return {"speech_bits": 244, "crc_bits": 8, "repetition_bits": 8,
            "padding_positions": 12, "crc_selectors_checked": 65,
            "packing_order_verified": False, "native_execution_verified": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.image.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(result)


if __name__ == "__main__":
    main()
