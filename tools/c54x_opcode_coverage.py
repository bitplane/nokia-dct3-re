#!/usr/bin/env python3
"""Summarize exact C54x opcode coverage from an observation-only trace."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys


OPCODE = re.compile(
    r"^\[opcov\] op=([0-9a-fA-F]{4}) first_pc=([0-9a-fA-F]{4})(?: count=(\d+))?$"
)
ASSERTION = re.compile(r"^(?:\[:\] )?\[opassert\] op=([0-9a-fA-F]{4})$")
ROM4_IDLE_OPCODE_COUNT = 457
ROM4_IDLE_GROUP_COUNT = 91
ROM4_IDLE_SET_SHA256 = "e5ab0413453f271100996a54cea8f712eebe6381d4f7b4f7bf959f55631efefc"


def summarize(text: str) -> dict[str, object]:
    first_pc = {}
    counts = {}
    asserted = set()
    for line in text.splitlines():
        match = OPCODE.match(line)
        if match:
            opcode, pc = (int(value, 16) for value in match.groups()[:2])
            first_pc.setdefault(opcode, pc)
            counts[opcode] = counts.get(opcode, 0) + int(match.group(3) or 1)
        match = ASSERTION.match(line)
        if match:
            asserted.add(int(match.group(1), 16))
    if not first_pc:
        raise ValueError("no [opcov] records")
    if asserted - first_pc.keys():
        raise ValueError("assertion marker names an opcode absent from the execution trace")
    opcodes = sorted(first_pc)
    encoded = b"".join(opcode.to_bytes(2, "big") for opcode in opcodes)
    return {
        "opcodes": len(opcodes),
        "high_byte_groups": len({opcode >> 8 for opcode in opcodes}),
        "set_sha256": hashlib.sha256(encoded).hexdigest(),
        "first_pc": first_pc,
        "counts": counts,
        "asserted": asserted,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--require-rom4-idle", action="store_true")
    parser.add_argument("--fixture-log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = summarize(args.log.read_text(errors="replace"))
        fixture = summarize(args.fixture_log.read_text(errors="replace")) if args.fixture_log else None
        if args.require_rom4_idle:
            actual = (result["opcodes"], result["high_byte_groups"], result["set_sha256"])
            expected = (ROM4_IDLE_OPCODE_COUNT, ROM4_IDLE_GROUP_COUNT,
                        ROM4_IDLE_SET_SHA256)
            if actual != expected:
                raise ValueError(f"ROM4 idle coverage mismatch: {actual!r}")
    except (OSError, ValueError) as error:
        print(f"C54x opcode coverage rejected: {error}", file=sys.stderr)
        return 1
    print(
        f"C54x opcode coverage: opcodes={result['opcodes']} "
        f"groups={result['high_byte_groups']} sha256={result['set_sha256']}"
    )
    if fixture:
        rom4 = set(result["first_pc"])
        fixture_only = set(fixture["first_pc"])
        uncovered = rom4 - fixture_only
        overlap = rom4 & fixture_only
        print(f"ROM4-only encodings: {len(uncovered)}; fixture overlap: {len(overlap)}; "
              f"fixture-only encodings: {len(fixture_only - rom4)}")
        asserted = overlap & fixture["asserted"]
        executed_only = overlap - asserted
        print(f"ROM4 fixture classes: asserted={len(asserted)} "
              f"executed-only={len(executed_only)} absent={len(uncovered)}")
        for opcode in sorted(executed_only,
                             key=lambda op: (-result["counts"][op], op))[:10]:
            print(f"  executed-only op={opcode:04x} first_pc={result['first_pc'][opcode]:04x} "
                  f"executions={result['counts'][opcode]}")
        for opcode in sorted(uncovered, key=lambda op: (-result["counts"][op], op))[:20]:
            print(f"  op={opcode:04x} first_pc={result['first_pc'][opcode]:04x} "
                  f"executions={result['counts'][opcode]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
