#!/usr/bin/env python3
"""Validate the MAD2 Timer-1 destination/FIQ5 controller contract."""

import argparse
import pathlib
import re
import sys


DESTINATION_RE = re.compile(
    r"mad2_timer1: event=destination counter=(?P<counter>[0-9a-f]{4}) "
    r"destination=(?P<destination>[0-9a-f]{4}) pending=(?P<pending>[0-9a-f]{3})",
    re.IGNORECASE,
)
ACK_RE = re.compile(r"mad2_timer: event=ack mask=(?P<mask>[0-9a-f]{3})", re.IGNORECASE)


def check(text):
    destinations = [
        {key: int(value, 16) for key, value in match.groupdict().items()}
        for match in DESTINATION_RE.finditer(text)
    ]
    acknowledgements = [int(match.group("mask"), 16) for match in ACK_RE.finditer(text)]
    errors = []
    if not destinations:
        errors.append("Timer-1 never reached its destination")
    if any(item["counter"] != 0x7FFF or item["destination"] != 0x7FFF for item in destinations):
        errors.append("Timer-1 destination event did not occur at 0x7fff")
    if any(not item["pending"] & 0x020 for item in destinations):
        errors.append("Timer-1 destination did not assert FIQ5/status bit 0x020")
    if 0x020 not in acknowledgements:
        errors.append("firmware did not acknowledge Timer-1 FIQ5/status bit 0x020")
    return errors, {
        "destinations": len(destinations),
        "fiq5_acknowledgements": acknowledgements.count(0x020),
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args(argv)
    errors, counts = check(args.log.read_text(errors="replace"))
    print(f"MAD2 Timer-1 contract: {counts}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
