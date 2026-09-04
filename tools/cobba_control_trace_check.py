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
    if matches != ["0f"]:
        raise ValueError(
            "COBBA control conformance result is "
            f"{matches or 'missing'}, expected exactly 0f"
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
    ]
    normalized = [tuple(field.lower() for field in item) for item in transactions]
    if normalized != expected:
        raise ValueError(
            "COBBA ordered control transactions are "
            f"{normalized or 'missing'}, expected {expected}"
        )
    return 0x0F


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
