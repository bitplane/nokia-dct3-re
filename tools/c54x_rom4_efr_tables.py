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

# TS 45.003 table 6: d(0)..d(259) select one-based preliminary-coded w bits.
# Numeric protocol data, transcribed independently of the acquired ROM table.
IMPORTANCE = (
    39,40,41,42,43,44,146,147,148,149,150,151,94,95,201,202,
    48,89,100,141,45,152,96,203,2,3,8,10,18,19,24,46,47,153,154,
    97,204,4,5,11,12,16,9,6,7,13,17,20,98,205,
    1,14,15,21,25,26,28,155,207,196,248,90,142,197,249,
    253,254,255,256,257,258,259,260,49,
    101,156,208,22,23,27,29,
    52,56,60,64,68,104,108,112,116,120,159,163,167,171,175,
    211,215,219,223,227,91,143,198,250,50,102,157,209,
    30,31,32,33,34,35,36,99,206,53,57,
    61,65,69,105,109,113,117,121,160,164,168,172,176,
    212,216,220,224,228,54,58,62,66,106,110,114,118,
    161,165,169,173,213,221,225,92,144,199,251,51,103,158,210,
    93,145,200,252,55,59,63,67,107,111,115,119,162,166,170,
    174,214,222,226,37,38,
    70,72,73,122,124,125,177,179,180,229,231,232,217,218,
    71,123,178,230,74,77,80,83,86,126,129,132,135,138,
    181,184,187,190,193,233,236,239,242,245,
    75,78,81,84,87,127,130,133,136,139,182,185,188,191,194,
    234,237,240,243,246,
    76,79,82,85,88,128,131,134,137,140,183,186,189,192,195,
    235,238,241,244,247)


def preliminary_descriptors():
    speech = speech_descriptors()
    result = []
    for position, descriptor in enumerate(speech, 1):
        result.append(descriptor)
        if position in (71, 121, 174, 224):
            result.extend([speech[position - 2]] * 2)
    # Native CRC is bit-reversed relative to the conventional polynomial remainder.
    result.extend((57 << 8) | (15 - bit) for bit in range(8))
    return tuple(result)


def native_positions():
    """Observed ROM storage permutation of normative d bits; None is padding."""
    positions = [bit for word in range(5)
                 for bit in range(word * 32 + 30, word * 32 - 1, -2)]
    positions.extend([179, 181, None, None, None])
    positions.extend(range(180, 159, -2))
    for start in (147, 115, 83, 51, 19):
        positions.extend(range(start, start + 32, 2))
    positions.extend([None] * 7)
    positions.extend(range(1, 18, 2))
    for start in (182, 198, 214, 230):
        positions.extend(range(start + 15, start - 1, -1))
    positions.extend([None, None])
    positions.extend(range(259, 245, -1))
    return tuple(positions)


def expected_packing_descriptors():
    preliminary = preliminary_descriptors()
    importance = tuple(preliminary[position - 1] for position in IMPORTANCE)
    return tuple(0x3907 if position is None else importance[position]
                 for position in native_positions())


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
    if words(0xd8c2, 272) != expected_packing_descriptors():
        raise ValueError("EFR packing descriptor order differs")
    if words(0xd9d2, 65) != expected_crc_selectors():
        raise ValueError("EFR CRC selector order differs")
    return {"speech_bits": 244, "crc_bits": 8, "repetition_bits": 8,
            "padding_positions": 12, "crc_selectors_checked": 65,
            "packing_order_verified": True, "native_execution_verified": False}


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
