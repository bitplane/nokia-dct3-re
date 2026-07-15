#!/usr/bin/env python3
"""Validate the observed MAD2 GENSIO/CCONT transaction grammar."""

import argparse
import pathlib
import re
import sys


ACCESS_RE = re.compile(
    r"gensio: ([RW]) off=([0-9a-fA-F]{2}) data=([0-9a-fA-F]{2})"
)


def parse_accesses(text):
    return [
        (direction, int(offset, 16), int(data, 16))
        for direction, offset, data in ACCESS_RE.findall(text)
    ]


def check_accesses(accesses):
    errors = []
    selected = None
    ccont_phase = None
    pending_read = False
    read_ready_seen = False
    read_count = 0
    write_count = 0

    for direction, offset, data in accesses:
        if direction == "W" and offset == 0x2D:
            selected = data
            if data == 0x25:
                ccont_phase = "command"
                pending_read = False
                read_ready_seen = False
            continue

        if selected != 0x25:
            continue

        if direction == "R" and offset == 0x6D:
            if pending_read and data == 0x07:
                read_ready_seen = True
            continue

        if direction == "W" and offset == 0x2C:
            if ccont_phase == "command":
                pending_read = bool(data & 0x04)
                read_ready_seen = False
                ccont_phase = "data"
            elif ccont_phase == "data":
                if pending_read:
                    errors.append("CCONT read command was followed by a data write")
                write_count += 1
                ccont_phase = "command"
            else:
                errors.append("CCONT byte observed without endpoint selection")
            continue

        if direction == "R" and offset == 0x6C:
            if ccont_phase != "data" or not pending_read:
                errors.append("CCONT data read without a pending read command")
            if not read_ready_seen:
                errors.append("CCONT data consumed before GENSIO status 0x07")
            read_count += 1
            pending_read = False
            read_ready_seen = False
            ccont_phase = "command"

    if not accesses:
        errors.append("no GENSIO trace records found")
    if read_count == 0:
        errors.append("no complete CCONT read transactions found")
    if write_count == 0:
        errors.append("no complete CCONT write transactions found")

    return errors, {"accesses": len(accesses), "reads": read_count, "writes": write_count}


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args(argv)

    accesses = parse_accesses(args.log.read_text(errors="replace"))
    errors, counts = check_accesses(accesses)
    print(
        "GENSIO contract: "
        f"{counts['accesses']} accesses, {counts['reads']} CCONT reads, "
        f"{counts['writes']} CCONT writes"
    )
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
