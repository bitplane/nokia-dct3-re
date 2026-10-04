#!/usr/bin/env python3
"""Check durable A/123 storage and the observed save or cold-start detail frame."""
import argparse
from hashlib import sha256
from pathlib import Path

from PIL import Image

try:
    from .sim_phonebook_check import validate_phonebook_storage
except ImportError:
    from sim_phonebook_check import validate_phonebook_storage

ORACLES = {
    "save": "740ad810ae85667208f4d02b29f349c30a042b91218d84622b0d39e3a6fa24f5",
    "readback": "39ca7b13f4afdc8c6e3ca553d7fd0bafcdd7dd3de42c054edf0f445713dd09bc",
}


def check(data, pixels, size, stage):
    validate_phonebook_storage(data, b"A")
    if size != (96, 60) or sha256(pixels).hexdigest() != ORACLES[stage]:
        raise ValueError(f"6250 {stage} frame differs from the reviewed physical-input capture")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", choices=ORACLES)
    parser.add_argument("nvram", type=Path)
    parser.add_argument("frame", type=Path)
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as frame:
            check(args.nvram.read_bytes(), frame.convert("L").tobytes(), frame.size, args.stage)
    except (OSError, ValueError) as error:
        parser.exit(1, f"6250 phonebook failed: {error}\n")
    print(f"6250 phonebook {args.stage} PASS: durable A/123 and reviewed frame")


if __name__ == "__main__":
    main()
