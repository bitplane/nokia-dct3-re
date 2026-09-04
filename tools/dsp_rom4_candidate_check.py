#!/usr/bin/env python3
"""Validate a privately supplied Nokia 5110 DSP ROM4 research pair."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys


PROGRAM_BYTES = 0x20000
HISTORICAL_PROGRAM_BYTES = PROGRAM_BYTES - 2
DROM_FIRST = 0xB000
DROM_LAST = 0xEFFF
DROM_LINE = re.compile(r"^\s*([0-9a-fA-F]+)\s*:\s*([0-9a-fA-F]+)\s*$")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_program(path: pathlib.Path) -> tuple[int, str]:
    data = path.read_bytes()
    if len(data) not in (HISTORICAL_PROGRAM_BYTES, PROGRAM_BYTES):
        raise ValueError(
            f"program image is {len(data)} bytes; expected the historical "
            f"{HISTORICAL_PROGRAM_BYTES}-byte export or a {PROGRAM_BYTES}-byte image"
        )
    if len(set(data)) == 1:
        raise ValueError(f"program image is uniform 0x{data[0]:02x} fill")
    return len(data), sha256(data)


def parse_drom(path: pathlib.Path) -> tuple[dict[int, int], str]:
    data = path.read_bytes()
    words: dict[int, int] = {}
    for number, raw in enumerate(data.decode("ascii").splitlines(), 1):
        if not raw.strip():
            continue
        match = DROM_LINE.fullmatch(raw)
        if not match:
            raise ValueError(f"DROM line {number} has an unsupported format")
        address, value = (int(item, 16) for item in match.groups())
        if not DROM_FIRST <= address <= DROM_LAST:
            raise ValueError(f"DROM line {number} address 0x{address:x} is outside B000-EFFF")
        if value > 0xFFFF:
            raise ValueError(f"DROM line {number} value 0x{value:x} is not a word")
        if address in words:
            raise ValueError(f"DROM address 0x{address:04x} is duplicated")
        words[address] = value
    expected = DROM_LAST - DROM_FIRST + 1
    if len(words) != expected or set(words) != set(range(DROM_FIRST, DROM_LAST + 1)):
        raise ValueError(f"DROM contains {len(words)} words; expected complete B000-EFFF ({expected})")
    if len(set(words.values())) == 1:
        raise ValueError(f"DROM is uniform 0x{next(iter(words.values())):04x} fill")
    return words, sha256(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("program", type=pathlib.Path)
    parser.add_argument("drom", type=pathlib.Path)
    args = parser.parse_args()
    try:
        size, program_hash = validate_program(args.program)
        words, drom_hash = parse_drom(args.drom)
    except (OSError, UnicodeError, ValueError) as error:
        print(f"ROM4 candidate rejected: {error}", file=sys.stderr)
        return 1
    suffix = " (historical export omits DSP word FFFF)" if size == HISTORICAL_PROGRAM_BYTES else ""
    print(f"program: nonuniform size={size} sha256={program_hash}{suffix}")
    print(f"drom: complete words={len(words)} sha256={drom_hash}")
    print("ROM4 candidate has the expected transport shape; provenance remains a separate requirement")
    return 0


if __name__ == "__main__":
    sys.exit(main())
