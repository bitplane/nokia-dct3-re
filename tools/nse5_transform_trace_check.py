#!/usr/bin/env python3
"""Check the observed ROM4 rotation helper against independent word arithmetic."""

import argparse
from pathlib import Path
import re


ROTATION = re.compile(
    r"nse5_compat_rotation: base=([0-9a-f]{4}) "
    r"input=([0-9a-f:]+) output=([0-9a-f:]+) other=([0-9a-f]{4}) "
    r"other_input=([0-9a-f:]+) other_output=([0-9a-f:]+) t=([0-9.]+)"
)


def rotate32(high: int, low: int, count: int) -> tuple[int, int]:
    value = (high << 16) | low
    value = ((value >> count) | (value << (32 - count))) & 0xffffffff
    return value >> 16, value & 0xffff


def check_trace(text: str) -> int:
    count = 0
    for match in ROTATION.finditer(text):
        before = tuple(int(word, 16) for word in match[2].split(":"))
        after = tuple(int(word, 16) for word in match[3].split(":"))
        other_before = tuple(int(word, 16) for word in match[5].split(":"))
        other_after = tuple(int(word, 16) for word in match[6].split(":"))
        if any(len(words) != 2 for words in (before, after, other_before, other_after)):
            raise ValueError("rotation record requires two words per operand")
        if after != rotate32(*before, 10) or other_after != rotate32(*other_before, 31):
            raise ValueError(f"rotation mismatch at {match[1]}, t={match[7]}")
        count += 1
    if not count:
        raise ValueError("no observed rotation helper records")
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    args = parser.parse_args()
    try:
        count = check_trace(args.trace.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f"NSE-5 transform check: {error}\n")
    print(f"NSE-5 rotation helper: {count} observed calls match word arithmetic")


if __name__ == "__main__":
    main()
