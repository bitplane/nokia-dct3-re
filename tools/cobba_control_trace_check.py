#!/usr/bin/env python3
"""Check the opaque DSP-to-COBBA serial-control transport contract."""

import argparse
from pathlib import Path
import re
import sys


def check(text: str) -> int:
    matches = re.findall(
        r"cobba_fixture: control_conformance=([0-9a-fA-F]{2})", text
    )
    if matches != ["1f"]:
        raise ValueError(
            "COBBA control conformance result is "
            f"{matches or 'missing'}, expected exactly 1f"
        )

    transactions = re.findall(
        r"cobba: control sequence=(\d+) direction=(read|write) "
        r"address=([0-9a-fA-F]) data=([0-9a-fA-F]{3}) ",
        text,
    )
    expected = [
        ("1", "write", "3", "ace"),
        ("2", "read", "3", "ace"),
        ("3", "write", "4", "123"),
        ("4", "write", "5", "fed"),
        ("5", "read", "5", "fed"),
        ("6", "write", "8", "610"),
        ("7", "write", "8", "000"),
    ]
    normalized = [tuple(field.lower() for field in item) for item in transactions]
    if normalized != expected:
        raise ValueError(
            "COBBA ordered control transactions are "
            f"{normalized or 'missing'}, expected {expected}"
        )
    loopbacks = re.findall(
        r"cobba: codec serial loopback data=([0-9a-fA-F]{4}) count=(\d+) ",
        text,
    )
    if [(data.lower(), count) for data, count in loopbacks] != [("0aaa", "1")]:
        raise ValueError(
            "COBBA codec serial loopback transactions are "
            f"{loopbacks or 'missing'}, expected exactly [('0aaa', '1')]"
        )
    return 0x1F


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"))
    except ValueError as exc:
        print(exc, file=sys.stderr)
        return 1
    print(f"COBBA control transport contract: {result:02x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
