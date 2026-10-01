#!/usr/bin/env python3
"""Extract the stock NSE-5 slow roller classifier's initialized patterns."""

import hashlib
import json
from pathlib import Path


FLASH_SHA1 = "53af8324919f455ba8199d2c05f7a921cfb811d5"
TABLE_OFFSET = 0x4FFBD8 - 0x200000
PHASE_CONTACTS = {1: (1, 2), 2: (0, 2), 3: (0, 1)}


def contact_levels(phase, driven_low):
    """Passive closed-pair candidate, with released pins pulled high."""
    levels = [1, 1, 1]
    levels[driven_low] = 0
    pair = PHASE_CONTACTS[phase]
    if driven_low in pair:
        for pin in pair:
            levels[pin] = 0
    return tuple(levels)


def probe_pattern(phase):
    return [contact_levels(phase, drive)[sense]
            for drive in range(3) for sense in range(3) if sense != drive]


def fast_phase(levels, previous):
    return {(1, 0, 0): 1, (0, 1, 0): 2, (0, 0, 1): 3}.get(
        tuple(levels), previous)


def thumb_bl_candidates(image, targets):
    """Scan every halfword for classic Thumb BL encodings; data may match."""
    result = {target: [] for target in targets}
    for offset in range(0, len(image) - 3, 2):
        first = int.from_bytes(image[offset:offset + 2], "big")
        second = int.from_bytes(image[offset + 2:offset + 4], "big")
        if first & 0xf800 != 0xf000 or second & 0xf800 != 0xf800:
            continue
        displacement = ((first & 0x7ff) << 12) | ((second & 0x7ff) << 1)
        if displacement & 0x400000:
            displacement -= 0x800000
        target = offset + 0x200000 + 4 + displacement
        if target in result:
            result[target].append(offset + 0x200000)
    return result


def extract(image):
    if hashlib.sha1(image).hexdigest() != FLASH_SHA1:
        raise ValueError("requires the acquired NSE-5 v5.01 PPM C flash")
    # The initialization record copies 62 bytes to 0x168a3c. The final
    # classifier row needs only its first six bytes, not trailing padding.
    header = image[TABLE_OFFSET - 8:TABLE_OFFSET]
    if header != bytes.fromhex("0000003e00168a3c"):
        raise ValueError("roller initialization record changed")
    # These are the PC-relative pools actually used by 0x473c80's three
    # restoration branches, not inferred register identities from labels.
    pools = {0x473FE8: 0x200B3, 0x473FEC: 0x20033,
             0x473FF0: 0x200B1, 0x473FF4: 0x200B2,
             0x473FF8: 0x20032, 0x473FFC: 0x20031,
             0x474000: 0x200F1, 0x4CACC0: 0x20000,
             0x4CACC4: 0x168A32, 0x4CACC8: 0x168A33,
             0x4CACCC: 0x168A34}
    for address, expected in pools.items():
        offset = address - 0x200000
        if int.from_bytes(image[offset:offset + 4], "big") != expected:
            raise ValueError(f"roller GPIO literal changed at {address:#x}")
    rows = [list(image[TABLE_OFFSET + 8 * i:TABLE_OFFSET + 8 * i + 6])
            for i in range(8)]
    callers = thumb_bl_candidates(image, (0x473F4C, 0x4741C2))
    if callers != {0x473F4C: [0x4CAA60], 0x4741C2: [0x4CAA2C]}:
        raise ValueError("unexpected roller/slide direct-call candidates")
    return {"flash_sha1": FLASH_SHA1, "destination": "0x168a3c",
            "probe_order": ["A:B", "A:C", "B:A", "B:C", "C:A", "C:B"],
            "patterns": rows, "unique_phase_rows": {"0": 1, "2": 2, "4": 3},
            "gpio_literals": {hex(a): hex(v) for a, v in pools.items()},
            "roller_direction": "B1/B2/B3 bit set releases pin; clear drives latch",
            "irq7_dispatcher": "0x4caa04",
            "direct_call_candidates": {hex(a): list(map(hex, sites))
                                       for a, sites in callers.items()},
            "halfword_positions_scanned": (len(image) - 2) // 2,
            "unmatched": "retain previous phase"}


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    args = parser.parse_args()
    print(json.dumps(extract(args.flash.read_bytes()), indent=2))
