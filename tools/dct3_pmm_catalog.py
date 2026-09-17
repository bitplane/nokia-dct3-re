#!/usr/bin/env python3
"""Inspect linked PMMCAT entries in DCT3 flash-backed product state."""

import argparse
from dataclasses import dataclass
from pathlib import Path


MAGIC = b"PMMCAT"
FIRST_RECORD = 0x20
ERASED_WORD = b"\xff\xff"


@dataclass(frozen=True)
class Entry:
    offset: int
    entry_type: int
    index: int
    flags: bytes
    payload: bytes
    next_offset: int


def parse_catalog(data: bytes) -> list[Entry]:
    if data[6:12] != MAGIC:
        raise ValueError("PMMCAT magic missing at offset 0x06")

    entries: list[Entry] = []
    seen: set[int] = set()
    offset = FIRST_RECORD
    while offset not in (0, 0xffff):
        if offset in seen:
            raise ValueError(f"PMMCAT record loop at {offset:#x}")
        seen.add(offset)
        if offset + 12 > len(data):
            raise ValueError(f"PMMCAT entry header exceeds image at {offset:#x}")
        if data[offset:offset + 2] == ERASED_WORD:
            break
        entry_type = int.from_bytes(data[offset:offset + 2], "big")
        index = int.from_bytes(data[offset + 2:offset + 4], "big")
        flags = data[offset + 4:offset + 8]
        length = int.from_bytes(data[offset + 8:offset + 10], "big")
        next_offset = int.from_bytes(data[offset + 10:offset + 12], "big")
        payload_start = offset + 12
        payload_end = payload_start + length
        if payload_end > len(data):
            raise ValueError(f"entry {entry_type:04x}/{index:04x} exceeds image")
        if next_offset not in (0, 0xffff) and next_offset != payload_end:
            raise ValueError(
                f"entry {entry_type:04x}/{index:04x} next {next_offset:#x} "
                f"!= end {payload_end:#x}")
        entries.append(
            Entry(offset, entry_type, index, flags, data[payload_start:payload_end], next_offset))
        offset = next_offset
    return entries


def find_catalogs(data: bytes) -> list[tuple[int, list[Entry]]]:
    catalogs = []
    start = 0
    while (magic := data.find(MAGIC, start)) >= 0:
        base = magic - 6
        if base >= 0:
            try:
                catalogs.append((base, parse_catalog(data[base:])))
            except ValueError:
                pass
        start = magic + len(MAGIC)
    return catalogs


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--type", action="append", type=lambda value: int(value, 0))
    parser.add_argument("--index", action="append", type=lambda value: int(value, 0))
    args = parser.parse_args()

    wanted_types = set(args.type or [])
    wanted_indices = set(args.index or [])
    catalogs = find_catalogs(args.image.read_bytes())
    for base, entries in catalogs:
        print(f"catalog={base:04x} entries={len(entries)}")
        for entry in entries:
            if wanted_types and entry.entry_type not in wanted_types:
                continue
            if wanted_indices and entry.index not in wanted_indices:
                continue
            print(
                f"  {base + entry.offset:04x} type={entry.entry_type:04x} "
                f"index={entry.index:04x} len={len(entry.payload):04x} "
                f"flags={entry.flags.hex()} next={entry.next_offset:04x} "
                f"data={entry.payload.hex()}")


if __name__ == "__main__":
    main()
