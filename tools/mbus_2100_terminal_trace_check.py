#!/usr/bin/env python3
"""Check the organic NAM-2 M2BUS terminal startup exchange."""

import re
import sys
from pathlib import Path


PHONE = re.compile(
    r"mbus_terminal: phone_frame type=([0-9a-f]{2}) command=([0-9a-f]{2}) "
    r"length=(\d+) sequence=([0-9a-f]{2}).* t=([0-9.]+)")
TX = re.compile(r"mbus_terminal: tx_complete type=([0-9a-f]{2}) length=(\d+)")


def check(text: str) -> None:
    events: list[tuple[str, str, str, int]] = []
    for line in text.splitlines():
        if match := PHONE.search(line):
            events.append(("phone", match[1], match[2], int(match[3])))
        elif match := TX.search(line):
            events.append(("terminal", match[1], "", int(match[2])))

    required = [
        ("phone", "d0", "01", 9),
        ("terminal", "7f", "", 6),
        ("terminal", "d0", "", 9),
        ("phone", "d0", "05", 9),
        ("terminal", "7f", "", 6),
    ]
    cursor = 0
    for expected in required:
        try:
            cursor = events.index(expected, cursor) + 1
        except ValueError as error:
            raise SystemExit(
                f"NAM-2 M2BUS exchange missing ordered event {expected}; "
                f"observed {events[:20]}") from error


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: mbus_2100_terminal_trace_check.py ERROR.LOG")
    check(Path(sys.argv[1]).read_text(errors="replace"))
    print("NAM-2 M2BUS terminal exchange: PASS D0/01 ACK D0/04 D0/05 ACK")


if __name__ == "__main__":
    main()
