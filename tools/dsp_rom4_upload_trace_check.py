#!/usr/bin/env python3
"""Check the observed ROM4 staged-upload lifecycle against its block catalogue."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

from dsp_rom_audit import parse_block_map


HEADER_RE = re.compile(r"HDRLOG #\d+ hdr7F=0x([0-9A-Fa-f]{4}) @([0-9]+)k")
REPLY_RE = re.compile(r"HDRLOG #\d+ reply=0x([0-9A-Fa-f]{4}) @([0-9]+)k")
REQUEST_RE = re.compile(r"REQ\[0x871\]=0x([0-9A-Fa-f]{4})")
DESCRIPTOR_RE = re.compile(
    r"hdr\[7B\.\.7F\]=((?:[0-9A-Fa-f]{4} ){4}[0-9A-Fa-f]{4})")


def check_trace(log: str, block_map: str) -> list[str]:
    errors: list[str] = []
    chunks = {block.chunk_length for block in parse_block_map(block_map)}

    for marker, description in (
        ("REALUP: DSP RELEASED", "cold DSP release"),
        ("REALUP: DSP HELD in reset", "post-upload reset assertion"),
        ("CPU reset + DARAM PRESERVED", "reset-preserved warm release"),
    ):
        if marker not in log:
            errors.append(f"missing {description}")

    requests = [int(match.group(1), 16) for match in REQUEST_RE.finditer(log)]
    if 0x12 not in requests:
        errors.append("missing loader request 0x0012")
    if 0x01 not in requests:
        errors.append("missing run-mode request 0x0001")

    descriptors = {
        tuple(int(value, 16) for value in match.group(1).split())
        for match in DESCRIPTOR_RE.finditer(log)
    }
    for descriptor in (
        (0xFD00, 0xFF80, 0x0244, 0x0500, 0x0078),
        (0x0D80, 0x1000, 0x0122, 0x0580, 0x01F4),
        (0x25B4, 0x1F80, 0x034E, 0x0130, 0x044C),
    ):
        if descriptor not in descriptors:
            errors.append(
                "missing upload descriptor "
                + " ".join(f"{value:04x}" for value in descriptor)
            )

    headers = [(int(value, 16), int(step)) for value, step in HEADER_RE.findall(log)]
    significant = [(value, step) for value, step in headers if value not in (0, 1)]
    expected = [0x0078, 0x01F4, 0x044C, 0x0078, 0x0118, 0x04EC]
    observed = [value for value, _ in significant]
    cursor = 0
    for value in observed:
        if cursor < len(expected) and value == expected[cursor]:
            cursor += 1
    if cursor != len(expected):
        errors.append(
            "missing ordered upload headers: expected "
            + " ".join(f"0x{value:04x}" for value in expected)
            + "; observed "
            + " ".join(f"0x{value:04x}" for value in observed)
        )
    unknown = sorted({value for value, _ in significant if value not in chunks})
    if unknown:
        errors.append(
            "upload headers absent from block catalogue: "
            + " ".join(f"0x{value:04x}" for value in unknown)
        )

    replies = [int(value, 16) for value, _ in REPLY_RE.findall(log)]
    for value in (2, 4):
        if value not in replies:
            errors.append(f"missing upload reply 0x{value:04x}")
    if "pc=0x0D80" not in log:
        errors.append("loader2 entry 0x0d80 was not observed")
    if "SEEDDARAM:" not in log:
        errors.append("trace does not expose the current flat-snapshot assist")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("block_map", type=pathlib.Path)
    args = parser.parse_args()
    errors = check_trace(
        args.log.read_text(encoding="utf-8", errors="replace"),
        args.block_map.read_text(encoding="utf-8"),
    )
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1
    print("OK - ROM4 cold upload, warm reset, loader2 and demand uploads are ordered")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
