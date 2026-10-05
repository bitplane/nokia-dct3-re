#!/usr/bin/env python3
"""Check physical 8850 SIM save or preserved-cold-boot contact presentation."""
import argparse
from hashlib import sha256
from pathlib import Path
import sys

from PIL import Image

try:
    from .sim_phonebook_check import validate_phonebook, validate_phonebook_storage
except ImportError:
    from sim_phonebook_check import validate_phonebook, validate_phonebook_storage

ORACLES = {
    "save": "6b4ca81a8f259f7446f55aaa38492199d473e06b61ed08f26b2cc0b47dcd889e",
    "readback": "d37d0b7ff6a5a269ede1867412511d22cbaeff061b0e223be58d1dc60d13cf9b",
}


def check(stage: str, trace: str, data: bytes, pixels: bytes, size: tuple[int, int]) -> None:
    if stage == "save":
        validate_phonebook(trace, data, b"A")
        if "8850_phonebook_physical: action=save" not in trace:
            raise ValueError("missing physical Save input")
    else:
        validate_phonebook_storage(data, b"A")
        if "ins=dc" in trace:
            raise ValueError("readback run rewrote SIM storage")
        if "ins=b2 p1=01 p2=04 p3=20 selected=6f3a" not in trace:
            raise ValueError("cold firmware did not read ADN record 1")
        if "8850_phonebook_read_physical: action=contact" not in trace:
            raise ValueError("missing physical contact-detail input")
    # Exclude softkeys; retain the whole width so all digits of 123 are checked.
    if size != (84, 48) or len(pixels) != 84 * 48:
        raise ValueError("unexpected LCD geometry or pixel count")
    if sha256(pixels[:84 * 36]).hexdigest() != ORACLES[stage]:
        raise ValueError(f"{stage} frame differs from the reviewed physical-input capture")


def main() -> int:
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
        print(f"8850 phonebook failed: {error}", file=sys.stderr)
        return 1
    print(f"8850 phonebook {args.stage} PASS: exact A/123 storage and reviewed frame")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
