#!/usr/bin/env python3
"""Classify local DSP ROM regions without treating fill files as firmware."""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import pathlib
import re
import sys


def classify(data: bytes) -> str:
    if not data:
        return "empty"
    if all(value == data[0] for value in data):
        return f"placeholder-fill-{data[0]:02x}"
    return "nonuniform-data"


def audit(path: pathlib.Path) -> tuple[str, int, str]:
    data = path.read_bytes()
    return classify(data), len(data), hashlib.sha256(data).hexdigest()


@dataclasses.dataclass(frozen=True)
class DspBlock:
    index: int
    source: int
    destination: int
    auxiliary: int
    length: int
    staging: int
    chunk_length: int
    flags: int

    @property
    def end(self) -> int:
        return self.destination + self.length


BLOCK_RE = re.compile(
    r"^\s*(?P<index>\d+):\s+\S+\s+"
    r"(?P<destination>[0-9A-Fa-f]{4})\s+"
    r"(?P<auxiliary>[0-9A-Fa-f]{4})\s+"
    r"(?P<length>[0-9A-Fa-f]{4})\s+"
    r"(?P<staging>[0-9A-Fa-f]{4})\s+"
    r"(?P<chunk_length>[0-9A-Fa-f]{4})\s+"
    r"(?P<flags>[0-9A-Fa-f]{4})"
)
SOURCE_RE = re.compile(r"\bDAT_(?P<source>[0-9A-Fa-f]+)\b")


def parse_block_map(text: str) -> list[DspBlock]:
    """Parse AlexD's recovered DSP block catalogue without assigning unknown fields."""
    blocks = []
    for line in text.splitlines():
        match = BLOCK_RE.match(line)
        source_match = SOURCE_RE.search(line)
        if not match or not source_match:
            continue
        values = {name: int(value, 16) for name, value in match.groupdict().items()
                  if name != "index"}
        blocks.append(DspBlock(
            index=int(match.group("index")),
            source=int(source_match.group("source"), 16),
            **values,
        ))
    return blocks


def covered_words(blocks: list[DspBlock], start: int, end: int) -> int:
    """Return union coverage in [start, end), clipping overlapping block ranges."""
    intervals = sorted(
        (max(start, block.destination), min(end, block.end))
        for block in blocks
        if block.length and block.destination < end and block.end > start
    )
    covered = 0
    cursor = start
    for lo, hi in intervals:
        if hi <= cursor:
            continue
        lo = max(lo, cursor)
        covered += hi - lo
        cursor = hi
    return covered


def print_block_map(path: pathlib.Path, start: int, end: int) -> int:
    blocks = parse_block_map(path.read_text(encoding="utf-8"))
    if not blocks:
        print(f"{path}: no DSP block records found", file=sys.stderr)
        return 2

    relevant = [block for block in blocks
                if block.length and block.destination < end and block.end > start]
    coverage = covered_words(relevant, start, end)
    span = end - start
    print(
        f"{path}: blocks={len(blocks)} range=0x{start:04x}..0x{end - 1:04x} "
        f"targeted_words={coverage}/{span} ({coverage / span:.1%})"
    )
    for block in relevant:
        print(
            f"  block={block.index:02d} source=0x{block.source:06x} "
            f"destination=0x{block.destination:04x}..0x{block.end - 1:04x} "
            f"words=0x{block.length:04x} auxiliary=0x{block.auxiliary:04x} "
            f"staging=0x{block.staging:04x} chunk=0x{block.chunk_length:04x}"
        )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="*", type=pathlib.Path)
    parser.add_argument("--block-map", type=pathlib.Path)
    parser.add_argument("--overlay-start", type=lambda value: int(value, 0), default=0x2000)
    parser.add_argument("--overlay-end", type=lambda value: int(value, 0), default=0x2800)
    args = parser.parse_args()

    if not args.paths and not args.block_map:
        parser.error("provide at least one ROM path or --block-map")

    status = 0
    for path in args.paths:
        if not path.is_file():
            print(f"{path}: missing")
            status = 2
            continue
        kind, size, digest = audit(path)
        print(f"{path}: {kind} size={size} sha256={digest}")
    if args.block_map:
        if not args.block_map.is_file():
            print(f"{args.block_map}: missing", file=sys.stderr)
            status = 2
        else:
            status = max(status, print_block_map(
                args.block_map, args.overlay_start, args.overlay_end))
    return status


if __name__ == "__main__":
    sys.exit(main())
