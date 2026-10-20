#!/usr/bin/env python3
"""Read the acquired NAM-2 flash algorithm selector, not a geometry spec."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


SHA1 = "b7e30deb4393a76d509f843d25ecf20881bc25bb"
TABLE = 0x33D748
BASE = 0x200000


def select_family(rows, manufacturer, device, third):
    """Mirror 0x305b0c: scan all ten rows and retain the last match."""
    selected = 0
    for family, *identifiers in rows:
        if all(expected == 0xFFFF or expected == actual
               for expected, actual in zip(identifiers, (manufacturer, device, third))):
            selected = family
    return selected


def inspect(image):
    if hashlib.sha1(image).hexdigest() != SHA1:
        raise ValueError("requires the unchanged acquired NAM-2 v5.21 image")
    rows = [struct.unpack_from(">4H", image, TABLE - BASE + index * 8)
            for index in range(10)]
    return {
        "sha1": SHA1,
        "selector": "0x305b0c",
        "table": f"0x{TABLE:x}",
        "rows": rows,
        "observed_id": [0xB0, 0xD0, 0],
        "selected_family": select_family(rows, 0xB0, 0xD0, 0),
        "family_1": {
            "source": "0x33e064", "length": 0x144,
            "destination": "0x1351f0",
            "program": "0x1351f1", "erase": "0x135239",
            "dispatch_slots": "0x10fac8",
        },
        "erase_geometry": "not established by algorithm selection",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.image.read_bytes())
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
