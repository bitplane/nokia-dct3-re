#!/usr/bin/env python3
"""Validate the organic EF_ADN write produced by the phonebook fixture."""

import argparse
import sys
from pathlib import Path


RECORD_LENGTH = 32
RECORD_COUNT = 50


def validate_phonebook(trace: str, data: bytes) -> None:
    if len(data) != RECORD_LENGTH * RECORD_COUNT:
        raise ValueError(f"SIM NVRAM has {len(data)} bytes, expected 1600")
    if "ins=dc p1=01 p2=04 p3=20 selected=6f3a" not in trace:
        raise ValueError("firmware did not issue absolute UPDATE RECORD for EF_ADN record 1")
    if "update fid=6f3a record=1 length=32" not in trace:
        raise ValueError("card did not commit the firmware's EF_ADN update")

    expected = bytearray([0xff] * RECORD_LENGTH)
    expected[0:3] = b"ADA"
    expected[18:22] = bytes((0x03, 0x81, 0x21, 0xF3))
    if data[:RECORD_LENGTH] != expected:
        actual = data[:RECORD_LENGTH].hex(" ")
        raise ValueError(f"record 1 is not the expected GSM 11.11 ADA/123 record: {actual}")
    if data[RECORD_LENGTH:] != bytes([0xff]) * (len(data) - RECORD_LENGTH):
        raise ValueError("the fixture modified more than one ADN record")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("nvram", type=Path)
    args = parser.parse_args()
    try:
        validate_phonebook(args.trace.read_text(errors="replace"), args.nvram.read_bytes())
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("OK - EF_ADN record 1 contains ADA/123 and the remaining 49 records are erased")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
