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


def extract(image):
    if hashlib.sha1(image).hexdigest() != FLASH_SHA1:
        raise ValueError("requires the acquired NSE-5 v5.01 PPM C flash")
    # The initialization record copies 62 bytes to 0x168a3c. The final
    # classifier row needs only its first six bytes, not trailing padding.
    header = image[TABLE_OFFSET - 8:TABLE_OFFSET]
    if header != bytes.fromhex("0000003e00168a3c"):
        raise ValueError("roller initialization record changed")
    rows = [list(image[TABLE_OFFSET + 8 * i:TABLE_OFFSET + 8 * i + 6])
            for i in range(8)]
    return {"flash_sha1": FLASH_SHA1, "destination": "0x168a3c",
            "probe_order": ["A:B", "A:C", "B:A", "B:C", "C:A", "C:B"],
            "patterns": rows, "unique_phase_rows": {"0": 1, "2": 2, "4": 3},
            "unmatched": "retain previous phase"}


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flash", type=Path)
    args = parser.parse_args()
    print(json.dumps(extract(args.flash.read_bytes()), indent=2))
