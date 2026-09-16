#!/usr/bin/env python3
"""Validate the organic EF_ADN write produced by the phonebook fixture."""

import argparse
import sys
from pathlib import Path


RECORD_LENGTH = 32
RECORD_COUNT = 50
ADN_LENGTH = RECORD_LENGTH * RECORD_COUNT
PRE_SMS_NVRAM_LENGTH = ADN_LENGTH
PRE_CHV_NVRAM_LENGTH = ADN_LENGTH + 11 + 9 + 16 + (10 * 176) + (2 * 44)
PRE_ACM_NVRAM_LENGTH = PRE_CHV_NVRAM_LENGTH + (2 * 8) + (2 * 8) + 2 + 2 + 1
CURRENT_NVRAM_LENGTH = PRE_ACM_NVRAM_LENGTH + 3
SUPPORTED_NVRAM_LENGTHS = {
    PRE_SMS_NVRAM_LENGTH,
    PRE_CHV_NVRAM_LENGTH,
    PRE_ACM_NVRAM_LENGTH,
    CURRENT_NVRAM_LENGTH,
}


def validate_phonebook(trace: str, data: bytes, expected_name: bytes = b"ADA") -> None:
    if len(data) not in SUPPORTED_NVRAM_LENGTHS:
        expected = ", ".join(str(length) for length in sorted(SUPPORTED_NVRAM_LENGTHS))
        raise ValueError(
            f"SIM NVRAM has {len(data)} bytes, expected one of {expected}"
        )
    if "ins=dc p1=01 p2=04 p3=20 selected=6f3a" not in trace:
        raise ValueError("firmware did not issue absolute UPDATE RECORD for EF_ADN record 1")
    if "update fid=6f3a record=1 length=32" not in trace:
        raise ValueError("card did not commit the firmware's EF_ADN update")

    expected = bytearray([0xff] * RECORD_LENGTH)
    if not expected_name or len(expected_name) > 18:
        raise ValueError("expected contact name must contain 1 to 18 bytes")
    expected[0 : len(expected_name)] = expected_name
    expected[18:22] = bytes((0x03, 0x81, 0x21, 0xF3))
    if data[:RECORD_LENGTH] != expected:
        actual = data[:RECORD_LENGTH].hex(" ")
        label = expected_name.decode("ascii", errors="replace")
        raise ValueError(
            f"record 1 is not the expected GSM 11.11 {label}/123 record: {actual}"
        )
    adn = data[:ADN_LENGTH]
    if adn[RECORD_LENGTH:] != bytes([0xff]) * (ADN_LENGTH - RECORD_LENGTH):
        raise ValueError("the fixture modified more than one ADN record")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=Path)
    parser.add_argument("nvram", type=Path)
    parser.add_argument("--expected-name", default="ADA")
    args = parser.parse_args()
    try:
        validate_phonebook(
            args.trace.read_text(errors="replace"),
            args.nvram.read_bytes(),
            args.expected_name.encode("ascii"),
        )
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(
        f"OK - EF_ADN record 1 contains {args.expected_name}/123 and the remaining "
        "49 records are erased"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
