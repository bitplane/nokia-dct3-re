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


def group_gaps(result: dict[str, object], fixture: dict[str, object]) -> list[tuple[int, int, int, int, int]]:
    """Rank high-byte groups by unasserted ROM4 executions."""
    counts = result["counts"]
    rom4 = set(result["first_pc"])
    fixture_words = set(fixture["first_pc"])
    asserted = fixture["asserted"] & rom4
    groups = {}
    for opcode in rom4 - asserted:
        group = opcode >> 8
        row = groups.setdefault(group, [0, 0, 0, 0])
        index = 0 if opcode in fixture_words else 2
        row[index] += 1
        row[index + 1] += counts[opcode]
    return sorted(((group, *row) for group, row in groups.items()),
                  key=lambda row: (-(row[2] + row[4]), row[0]))


def ranked_gaps(result: dict[str, object], fixture: dict[str, object]) -> list[tuple[int, int, int, str]]:
    """List unasserted ROM4 words in observed execution order."""
    fixture_words = set(fixture["first_pc"])
    asserted = fixture["asserted"]
    rows = ((opcode, result["first_pc"][opcode], count,
             "executed-only" if opcode in fixture_words else "absent")
            for opcode, count in result["counts"].items() if opcode not in asserted)
    return sorted(rows, key=lambda row: (-row[2], row[0]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--require-rom4-idle", action="store_true")
    parser.add_argument("--fixture-log", type=pathlib.Path)
    parser.add_argument("--group-report", action="store_true",
                        help="rank unasserted ROM4 executions by opcode high byte")
    parser.add_argument("--all-gaps", action="store_true",
                        help="list every unasserted ROM4 word by observed execution count")
    parser.add_argument("--require-all-asserted", action="store_true",
                        help="fail unless every observed ROM4 word has a fixture assertion")
    args = parser.parse_args()
    if args.group_report and not args.fixture_log:
        parser.error("--group-report requires --fixture-log")
    if args.require_all_asserted and not args.fixture_log:
        parser.error("--require-all-asserted requires --fixture-log")
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
        if args.all_gaps:
            for opcode, pc, count, status in ranked_gaps(result, fixture):
                print(f"  {status} op={opcode:04x} first_pc={pc:04x} executions={count}")
        else:
            for opcode in sorted(executed_only,
                                 key=lambda op: (-result["counts"][op], op))[:10]:
                print(f"  executed-only op={opcode:04x} first_pc={result['first_pc'][opcode]:04x} "
                      f"executions={result['counts'][opcode]}")
            for opcode in sorted(uncovered, key=lambda op: (-result["counts"][op], op))[:20]:
                print(f"  op={opcode:04x} first_pc={result['first_pc'][opcode]:04x} "
                      f"executions={result['counts'][opcode]}")
        if args.group_report:
            print("ROM4 high-byte gaps (group, executed-only words/executions, "
                  "absent words/executions):")
            for group, executed_words, executed_count, absent_words, absent_count in group_gaps(result, fixture):
                print(f"  {group:02x}: {executed_words}/{executed_count} "
                      f"{absent_words}/{absent_count}")
        if args.require_all_asserted and (executed_only or uncovered):
            print("C54x opcode coverage rejected: observed ROM4 words lack fixture assertions",
                  file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
