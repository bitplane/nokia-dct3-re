#!/usr/bin/env python3
"""Validate the load-bearing ROM4 C54x challenge-transform boundary."""

from __future__ import annotations

import argparse
import pathlib
import re
import struct
import sys


TRACE = re.compile(
    r"^\[cmp\] (?P<step>\d+) pc=(?P<pc>[0-9a-fA-F]{4}) "
    r"op=(?P<op>[0-9a-fA-F]{4}).*? ar2=(?P<ar2>[0-9a-fA-F]{4}) "
    r"ar3=(?P<ar3>[0-9a-fA-F]{4}) ar4=(?P<ar4>[0-9a-fA-F]{4})"
)

EXPECTED_OPCODES = {
    0x4B73: 0xF074,
    0x7F2D: 0x7712,
    0x7FF5: 0xF072,
    0x7FF7: 0xF074,
    0x7FFA: 0x108A,
    0x8000: 0xFC00,
}

EXPECTED_RESPONSE = (
    0x3532, 0x0000, 0xFFFF, 0xFFFF, 0xFF0F, 0x0000, 0x0078,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x087C, 0x0000,
)


def parse_trace(text: str) -> list[dict[str, int]]:
    result = []
    for line in text.splitlines():
        match = TRACE.match(line)
        if match:
            result.append({key: int(value, 10 if key == "step" else 16)
                           for key, value in match.groupdict().items()})
    if not result:
        raise ValueError("no [cmp] C54x instruction records")
    return result


def check_trace(records: list[dict[str, int]]) -> dict[str, int]:
    seen: dict[int, list[dict[str, int]]] = {}
    for record in records:
        seen.setdefault(record["pc"], []).append(record)
    for pc, opcode in EXPECTED_OPCODES.items():
        if pc not in seen:
            raise ValueError(f"missing transform PC {pc:04x}")
        if any(record["op"] != opcode for record in seen[pc]):
            raise ValueError(f"opcode mismatch at {pc:04x}: expected {opcode:04x}")
    calls, entries, returns = seen[0x7FF7], seen[0x7FFA], seen[0x8000]
    if not (len(calls) == len(entries) == len(returns) == 6):
        raise ValueError(
            "RPTB/CALL contract mismatch: expected six calls, entries and returns, "
            f"got {len(calls)}/{len(entries)}/{len(returns)}"
        )
    if [record["ar4"] for record in entries] != list(range(0x1208, 0x1202, -1)):
        raise ValueError("RPTB/CALL destination walk did not cover 1208..1203")
    if len(seen[0x7FF5]) != 1:
        raise ValueError("challenge transform unexpectedly re-entered RPTB setup")
    return {
        "records": len(records),
        "rptb_calls": len(calls),
        "transform_entries": len(seen[0x7F2D]),
    }


def read_response(path: pathlib.Path, base_address: int = 0x0800) -> tuple[int, ...]:
    data = path.read_bytes()
    if base_address > 0x1200:
        raise ValueError(f"data-memory base {base_address:04x} is above response")
    offset = (0x1200 - base_address) * 2
    end = offset + len(EXPECTED_RESPONSE) * 2
    if len(data) < end:
        raise ValueError(f"data-memory image is too short: {len(data)} bytes")
    return struct.unpack_from(f">{len(EXPECTED_RESPONSE)}H", data, offset)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("trace", type=pathlib.Path)
    parser.add_argument("data_memory", type=pathlib.Path)
    parser.add_argument("--data-base", type=lambda value: int(value, 0), default=0x0800)
    args = parser.parse_args()
    try:
        summary = check_trace(parse_trace(args.trace.read_text(errors="replace")))
        response = read_response(args.data_memory, args.data_base)
        if response != EXPECTED_RESPONSE:
            raise ValueError("challenge response mismatch: " +
                             " ".join(f"{word:04x}" for word in response))
    except (OSError, ValueError) as error:
        print(f"ROM4 C54x transform rejected: {error}", file=sys.stderr)
        return 1
    print(
        "ROM4 C54x transform: "
        f"records={summary['records']} rptb_calls={summary['rptb_calls']} "
        f"entries={summary['transform_entries']} response=accepted"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
