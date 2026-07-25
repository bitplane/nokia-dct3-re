#!/usr/bin/env python3
"""Verify that each firmware DSP-memory fragment reaches DSP-owned memory."""

import pathlib
import re
import sys


PACKET = re.compile(
    r"TX consume type=51 payload=(?P<length>\d+).*?"
    r"data=(?P<data>[0-9a-f]+) t=",
    re.IGNORECASE,
)
APPLIED = re.compile(
    r"dsp_hle: data memory upload first=(?P<first>[0-9a-f]{4}) "
    r"words=(?P<words>\d+) last=(?P<last>[0-9a-f]{4}) t=",
    re.IGNORECASE,
)


def verify(text: str) -> dict[str, int]:
    packets = []
    for match in PACKET.finditer(text):
        payload = bytes.fromhex(match.group("data"))
        if len(payload) != int(match.group("length")):
            raise ValueError("type-0x51 payload length mismatch")
        first = int.from_bytes(payload[:2], "big")
        words = (len(payload) - 2) // 2
        packets.append((first, words, first + words - 1))
    applied = [
        (int(match.group("first"), 16), int(match.group("words")),
         int(match.group("last"), 16))
        for match in APPLIED.finditer(text)
    ]
    if not packets:
        raise ValueError("no type-0x51 DSP-memory packets")
    if packets != applied:
        raise ValueError("DSP-memory applied fragments differ from firmware packets")
    for previous, current in zip(applied, applied[1:]):
        if previous[2] + 1 != current[0]:
            raise ValueError("DSP-memory fragments are not contiguous")
    return {
        "first": applied[0][0],
        "last": applied[-1][2],
        "words": sum(item[1] for item in applied),
        "fragments": len(applied),
    }


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: dsp_memory_upload_trace_check.py MAME_ERROR_LOG")
    try:
        result = verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        f"OK - {result['fragments']} DSP-memory fragments populated "
        f"0x{result['first']:04x}-0x{result['last']:04x} "
        f"({result['words']} words)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
