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
DECODE = re.compile(r"^\[opdecode\] op=([0-9a-fA-F]{4}) supported=([01]) words=(\d+)$")
EXTENSIONS = re.compile(
    r"^\[opwords\] op=([0-9a-fA-F]{4}) words=(\d+) "
    r"ext1=([0-9a-fA-F]{4}) ext2=([0-9a-fA-F]{4}) "
    r"first_pc=([0-9a-fA-F]{6}) count=(\d+)$"
)
TUPLE_DECODE = re.compile(
    r"^\[optupledecode\] op=([0-9a-fA-F]{4}) words=(\d+) "
    r"ext1=([0-9a-fA-F]{4}) ext2=([0-9a-fA-F]{4}) "
    r"supported=([01]) decoded_words=(\d+)$"
)
# Current core corrections; two isolated cold controls and full assertions are
# documented in rom4_dsp_loader.md. This is an exact trace, not ISA completeness.
ROM4_IDLE_OPCODE_COUNT = 626
ROM4_IDLE_GROUP_COUNT = 110
ROM4_IDLE_SET_SHA256 = "05356c59d7b0843b8023b7a68822b1b258a2a18cd161db6dd33955580bf33a59"
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
    decoded = {}
    extensions = {}
    tuple_decoded = {}
    for line in text.splitlines():
        match = OPCODE.match(line)
        if line.startswith("[opcov]") and not match:
            raise ValueError("malformed opcode execution record")
        if match:
            opcode, pc = (int(value, 16) for value in match.groups()[:2])
            if match.group(3) is not None and not int(match.group(3)):
                raise ValueError("opcode execution count must be positive")
            first_pc.setdefault(opcode, pc)
            counts[opcode] = counts.get(opcode, 0) + int(match.group(3) or 1)
        match = ASSERTION.match(line)
        if line.startswith(("[opassert]", "[:] [opassert]")) and not match:
            raise ValueError("malformed opcode assertion record")
        if match:
            asserted.add(int(match.group(1), 16))
        match = DECODE.match(line)
        if line.startswith("[opdecode]") and not match:
            raise ValueError("malformed opcode decoder record")
        if match:
            opcode = int(match.group(1), 16)
            value = (bool(int(match.group(2))), int(match.group(3)))
            if (value[0] and not 1 <= value[1] <= 3) or (not value[0] and value[1]):
                raise ValueError("invalid supported/length decoder record")
            if opcode in decoded and decoded[opcode] != value:
                raise ValueError("conflicting decoder records")
            decoded[opcode] = value
        match = EXTENSIONS.match(line)
        if line.startswith("[opwords]") and not match:
            raise ValueError("malformed fetched-extension record")
        if match:
            opcode, words, ext1, ext2, pc, count = match.groups()
            opcode, ext1, ext2, pc = (int(value, 16) for value in (opcode, ext1, ext2, pc))
            words, count = int(words), int(count)
            if opcode >> 8 != 0x6f or words != (3 if opcode & 255 >= 0xe0 else 2):
                raise ValueError("invalid fetched-extension family or word count")
            if (words == 2 and ext2) or pc > 0x7fffff or not count:
                raise ValueError("invalid fetched-extension payload, address or count")
            key = (opcode, words, ext1, ext2)
            previous = extensions.get(key, (pc, 0))
            extensions[key] = (previous[0], previous[1] + count)
        match = TUPLE_DECODE.match(line)
        if line.startswith("[optupledecode]") and not match:
            raise ValueError("malformed tuple decoder record")
        if match:
            opcode, words, ext1, ext2, supported, decoded_words = match.groups()
            key = (int(opcode, 16), int(words), int(ext1, 16), int(ext2, 16))
            value = (bool(int(supported)), int(decoded_words))
            if (value[0] and value[1] != key[1]) or (not value[0] and value[1]):
                raise ValueError("invalid tuple decoder length")
            if key in tuple_decoded and tuple_decoded[key] != value:
                raise ValueError("conflicting tuple decoder records")
            tuple_decoded[key] = value
    if not first_pc:
        raise ValueError("no [opcov] records")
    if asserted - first_pc.keys():
        raise ValueError("assertion marker names an opcode absent from the execution trace")
    if decoded.keys() - first_pc.keys():
        raise ValueError("decoder record names an opcode absent from the execution trace")
    if {key[0] for key in extensions} - first_pc.keys():
        raise ValueError("fetched-extension record names an opcode absent from the execution trace")
    if tuple_decoded.keys() - extensions.keys():
        raise ValueError("tuple decoder record names an absent fetched tuple")
    opcodes = sorted(first_pc)
    encoded = b"".join(opcode.to_bytes(2, "big") for opcode in opcodes)
    return {
        "opcodes": len(opcodes),
        "high_byte_groups": len({opcode >> 8 for opcode in opcodes}),
        "set_sha256": hashlib.sha256(encoded).hexdigest(),
        "first_pc": first_pc,
        "counts": counts,
        "asserted": asserted,
        "decoded": decoded,
        "extensions": extensions,
        "tuple_decoded": tuple_decoded,
    }


def require_extension_capture(result: dict[str, object]) -> None:
    expected = {opcode for opcode in result["first_pc"] if opcode >> 8 == 0x6f}
    counts = {}
    for (opcode, _, _, _), (_, count) in result["extensions"].items():
        counts[opcode] = counts.get(opcode, 0) + count
    if not expected or counts.keys() != expected:
        raise ValueError("extension report requires captured tuples for every observed 6f opcode")
    if any(count != result["counts"][opcode] for opcode, count in counts.items()):
        raise ValueError("fetched-extension counts do not match opcode executions")


def require_extension_decoding(result: dict[str, object]) -> None:
    require_extension_capture(result)
    if result["tuple_decoded"].keys() != result["extensions"].keys():
        raise ValueError("tuple decoding requires decoder records for every fetched variant")
    if any(not valid for valid, _ in result["tuple_decoded"].values()):
        raise ValueError("captured instruction tuple remains undecoded")


def decoded_boundaries(result: dict[str, object]) -> set[int]:
    """Require real, complete variants for extension-controlled first words."""
    supported = {opcode for opcode, (valid, _) in result["decoded"].items()
                 if valid and opcode >> 8 != 0x6f}
    for opcode in result["first_pc"]:
        if opcode >> 8 != 0x6f:
            continue
        variants = {key: count for key, (_, count) in result["extensions"].items()
                    if key[0] == opcode}
        if (variants and sum(variants.values()) == result["counts"][opcode]
                and all(result["tuple_decoded"].get(key, (False, 0))[0] for key in variants)):
            supported.add(opcode)
    return supported


def require_rom4_idle(result: dict[str, object]) -> None:
    actual = (result["opcodes"], result["high_byte_groups"], result["set_sha256"])
    expected = (ROM4_IDLE_OPCODE_COUNT, ROM4_IDLE_GROUP_COUNT, ROM4_IDLE_SET_SHA256)
    if actual != expected:
        raise ValueError(f"ROM4 idle coverage mismatch: {actual!r}")


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
    parser.add_argument("--additional-fixture-log", type=pathlib.Path, action="append", default=[],
                        help="include another independently completed CPU fixture in assertion coverage")
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
    parser.add_argument("--frontend-report", action="store_true",
                        help="require actual decoder records and rank unsupported instruction boundaries")
    parser.add_argument("--extension-report", action="store_true",
                        help="require and list actual fetched 6f instruction tuples, not synthetic extensions")
    parser.add_argument("--require-extension-decoded", action="store_true",
                        help="require supported, exact-length decoding of every fetched 6f variant")
    parser.add_argument("--require-all-asserted", action="store_true",
                        help="fail unless every observed ROM4 word has a fixture assertion")
    args = parser.parse_args()
    if args.additional_fixture_log and not args.fixture_log:
        parser.error("--additional-fixture-log requires --fixture-log")
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
        if args.frontend_report and result["decoded"].keys() != result["first_pc"].keys():
            raise ValueError("frontend report requires decoder records for every observed opcode")
        if args.extension_report:
            require_extension_capture(result)
        if args.require_extension_decoded:
            require_extension_decoding(result)
        fixture = None
        if args.fixture_log:
            fixture_texts = [path.read_text(errors="replace") for path in [args.fixture_log, *args.additional_fixture_log]]
            # An assertion must have execution evidence in its own run, not
            # accidentally acquire that evidence from a different fixture.
            for text in fixture_texts:
                summarize(text)
            fixture = summarize("\n".join(fixture_texts))
        declared = decoder_declared_words(args.decoder_source.read_text()) if args.decoder_source else None
        if args.require_rom4_idle:
            require_rom4_idle(primary)
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
    if args.extension_report:
        print(f"Fetched 6f instruction tuples: {len(result['extensions'])} "
              "(execution fetches only; not physical pipeline timing)")
        for (opcode, words, ext1, ext2), (pc, count) in sorted(result["extensions"].items()):
            print(f"  op={opcode:04x} words={words} ext1={ext1:04x} ext2={ext2:04x} "
                  f"first_pc={pc:06x} executions={count}")
        supported = {key for key, (valid, _) in result["tuple_decoded"].items() if valid}
        print(f"Fetched frontend tuples: decoded={len(supported)} "
              f"observed={len(result['extensions'])} (not physical pipeline timing)")
    if args.frontend_report:
        supported = {opcode for opcode, (valid, _) in result["decoded"].items() if valid}
        missing = set(result["first_pc"]) - supported
        print(f"Frontend boundaries: decoded={len(supported)} unsupported={len(missing)} "
              f"observed={result['opcodes']} (synthetic extensions, not actual 6f lengths or stage timing)")
        for opcode in sorted(missing, key=lambda word: (-result["counts"][word], word)):
            print(f"  undecoded op={opcode:04x} first_pc={result['first_pc'][opcode]:04x} "
                  f"executions={result['counts'][opcode]} length=unknown")
        if args.extension_report:
            boundaries = decoded_boundaries(result)
            print(f"Observed instruction boundaries: decoded={len(boundaries)} "
                  f"observed={result['opcodes']} fetched_variants={len(result['extensions'])} "
                  "(not ISA completeness or physical pipeline timing)")
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
