#!/usr/bin/env python3
"""Check reviewed 8810 physical save/recall frames and persistent A/123 storage.

This does not claim an APDU trace, stable native operation, or fitted DSP identity.
"""
import argparse
from hashlib import sha256
from pathlib import Path
import sys

from PIL import Image

try:
    from .sim_phonebook_check import validate_phonebook_storage
except ImportError:
    from sim_phonebook_check import validate_phonebook_storage

ORACLES = {
    "save": "c0701126a0b251a6ae542c05fe44f58c921ddea4db30ddf50181d0dceb6c0377",
    "recall": "08731884907b0d32d6820468be1b0ab99bdb9d7956b6d8d6434b7e661d0ffcf1",
}
CONTACTS = {
    "save": "nse6_names_contact: label=Left Softkey t=31.",
    "recall": "nse6_phonebook_recall_contact: label=Left Softkey t=24.",
}


def check(stage, trace, data, pixels, size):
    validate_phonebook_storage(data, b"A")
    if CONTACTS[stage] not in trace:
        raise ValueError("missing physical save/detail input")
    if "unimplemented C54x opcode" in trace:
        raise ValueError("native execution encountered an unsupported opcode")
    if size != (84, 48) or len(pixels) != 84 * 48:
        raise ValueError("unexpected LCD geometry or pixel count")
    if sha256(pixels).hexdigest() != ORACLES[stage]:
        raise ValueError("frame differs from reviewed save/number capture")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=ORACLES)
    parser.add_argument("trace", type=Path)
    parser.add_argument("nvram", type=Path)
    parser.add_argument("frame", type=Path)
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as frame:
            check(args.stage, args.trace.read_text(errors="replace"),
                  args.nvram.read_bytes(), frame.convert("L").tobytes(), frame.size)
    except (OSError, ValueError) as error:
        print(f"8810 phonebook: FAIL {error}", file=sys.stderr)
        return 1
    print(f"8810 phonebook {args.stage}: PASS physical presentation and A/123 card storage")
    return 0


if __name__ == "__main__":
    sys.exit(main())
