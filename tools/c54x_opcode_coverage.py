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
# Free-running CTSI/absolute-compare baseline; provenance is in rom4_dsp_loader.md.
ROM4_IDLE_OPCODE_COUNT = 594
ROM4_IDLE_GROUP_COUNT = 107
ROM4_IDLE_SET_SHA256 = "5ec81f25976d365d2bbfe09037d70ac44676b5803535892d8237744c6a167d4e"
DECODER_MASK = re.compile(r"\(op\s*&\s*0x([0-9a-fA-F]+)\)\s*==\s*0x([0-9a-fA-F]+)")
DECODER_EXACT = re.compile(r"\bop\s*==\s*0x([0-9a-fA-F]{4})\b")
DECODER_CASE = re.compile(r"\bcase\s+0x([0-9a-fA-F]{4})\s*:")


def decoder_declared_words(source: str) -> set[int]:
    """Overapproximate top-level decoder matches; nested validity is not proven."""
    start = source.index("void tms320c54x_device::execute_one(u16 op)")
    end = source.index("void tms320c54x_device::execute_run()", start)
    body = source[start:end]
    masks = [(int(mask, 16), int(value, 16))
             for mask, value in DECODER_MASK.findall(body)]
    exact = {int(value, 16) for value in DECODER_EXACT.findall(body)}
    grouped_switch = body.index("switch (op & 0xff00)")
    exact_switch = body.index("switch (op)", grouped_switch)
    masks.extend((0xff00, int(value, 16))
                 for value in DECODER_CASE.findall(body[grouped_switch:exact_switch]))
    exact.update(int(value, 16)
                 for value in DECODER_CASE.findall(body[exact_switch:]))
    if not masks or not exact:
        raise ValueError("decoder cases or masks not found")
    return exact | {opcode for opcode in range(0x10000)
                    if any(opcode & mask == value for mask, value in masks)}


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


def ranked_variant_candidates(result: dict[str, object], fixture: dict[str, object],
                              declared: set[int]) -> list[tuple[int, int, int, int, int, int]]:
    """Rank static candidates by observed use after discounting one dominant word."""
    counts = result["counts"]
    fixture_words = set(fixture["first_pc"])
    untested = declared - fixture_words
    rows = []
    for group in {opcode >> 8 for opcode in counts}:
        candidates = sum(opcode >> 8 == group for opcode in untested)
        if candidates:
            observed_counts = [(opcode, count) for opcode, count in counts.items()
                               if opcode >> 8 == group]
            executions = sum(count for _, count in observed_counts)
            dominant, dominant_count = max(observed_counts, key=lambda row: (row[1], -row[0]))
            rows.append((group, executions - dominant_count, executions,
                         dominant, len(observed_counts), candidates))
    return sorted(rows, key=lambda row: (-row[1], -row[2], -row[5], row[0]))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--additional-log", type=pathlib.Path, action="append", default=[],
                        help="include another observed ROM4 trace in coverage")
    parser.add_argument("--require-rom4-idle", action="store_true")
    parser.add_argument("--fixture-log", type=pathlib.Path)
    parser.add_argument("--decoder-source", type=pathlib.Path,
                        help="report source-declared opcode matches absent from the fixture")
    parser.add_argument("--group-report", action="store_true",
                        help="rank unasserted ROM4 executions by opcode high byte")
    parser.add_argument("--variant-report", action="store_true",
                        help="rank untested static decoder matches by observed ROM4 family use")
    parser.add_argument("--observed-group", type=lambda value: int(value, 16),
                        action="append", default=[], metavar="HEX",
                        help="list exact observed words and counts in a high-byte group")
    parser.add_argument("--all-gaps", action="store_true",
                        help="list every unasserted ROM4 word by observed execution count")
    parser.add_argument("--require-all-asserted", action="store_true",
                        help="fail unless every observed ROM4 word has a fixture assertion")
    args = parser.parse_args()
    if args.group_report and not args.fixture_log:
        parser.error("--group-report requires --fixture-log")
    if args.require_all_asserted and not args.fixture_log:
        parser.error("--require-all-asserted requires --fixture-log")
    if args.decoder_source and not args.fixture_log:
        parser.error("--decoder-source requires --fixture-log")
    if args.variant_report and not args.decoder_source:
        parser.error("--variant-report requires --decoder-source")
    if any(group < 0 or group > 0xff for group in args.observed_group):
        parser.error("--observed-group must be a hexadecimal byte")
    try:
        primary_text = args.log.read_text(errors="replace")
        primary = summarize(primary_text)
        additional_texts = [(path, path.read_text(errors="replace"))
                            for path in args.additional_log]
        additional = [(path, summarize(text)) for path, text in additional_texts]
        result = summarize("\n".join([primary_text] +
                                     [text for _, text in additional_texts]))
        fixture = summarize(args.fixture_log.read_text(errors="replace")) if args.fixture_log else None
        declared = decoder_declared_words(args.decoder_source.read_text()) if args.decoder_source else None
        if args.require_rom4_idle:
            actual = (primary["opcodes"], primary["high_byte_groups"], primary["set_sha256"])
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
    for path, extra in additional:
        new = set(extra["first_pc"]) - set(primary["first_pc"])
        print(f"  additional ROM4 trace {path}: opcodes={extra['opcodes']} "
              f"new_vs_idle={len(new)}")
    for group in sorted(set(args.observed_group)):
        print(f"Observed ROM4 words in group {group:02x} (exact execution counts):")
        words = sorted(((opcode, count) for opcode, count in result["counts"].items()
                        if opcode >> 8 == group), key=lambda row: (-row[1], row[0]))
        for opcode, count in words:
            fixture_class = ("asserted" if fixture and opcode in fixture["asserted"]
                             else "executed-only" if fixture and opcode in fixture["first_pc"]
                             else "absent" if fixture else "unclassified")
            print(f"  op={opcode:04x} first_pc={result['first_pc'][opcode]:04x} "
                  f"executions={count} fixture={fixture_class}")
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
        if declared is not None:
            untested = declared - fixture_only
            groups = {}
            for opcode in untested:
                groups[opcode >> 8] = groups.get(opcode >> 8, 0) + 1
            no_fixture_groups = sorted(group for group in groups
                                       if not any(opcode >> 8 == group for opcode in fixture_only))
            print(f"Decoder-declared words: {len(declared)}; fixture-executed: "
                  f"{len(declared & fixture_only)}; not fixture-executed: {len(untested)}")
            print("  Static match only: nested validity and behavior are not verified")
            print(f"  High-byte groups with no fixture word: {len(no_fixture_groups)}")
            for group in no_fixture_groups[:24]:
                print(f"  no-fixture group={group:02x} static_matches={groups[group]}")
            if args.variant_report:
                print("Untested static-match candidates in ROM4-observed high-byte groups "
                      "(dominant word discounted; not validated instruction encodings):")
                for group, residual, executions, dominant, observed, candidates in ranked_variant_candidates(
                        result, fixture, declared):
                    print(f"  group={group:02x} non_dominant_executions={residual} "
                          f"rom4_executions={executions} dominant_word={dominant:04x} "
                          f"observed_words={observed} untested_static_matches={candidates}")
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
