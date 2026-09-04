#!/usr/bin/env python3
"""Summarize exact C54x opcode coverage from an observation-only trace."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys


OPCODE = re.compile(r"^\[opcov\] op=([0-9a-fA-F]{4}) first_pc=([0-9a-fA-F]{4})$")
ROM4_IDLE_OPCODE_COUNT = 628
ROM4_IDLE_GROUP_COUNT = 112
ROM4_IDLE_SET_SHA256 = "63ae45c872ccea39112898bfde7f5926acd675701675a860d765a66a35815d34"


def summarize(text: str) -> dict[str, object]:
    first_pc = {}
    for line in text.splitlines():
        match = OPCODE.match(line)
        if match:
            opcode, pc = (int(value, 16) for value in match.groups())
            first_pc.setdefault(opcode, pc)
    if not first_pc:
        raise ValueError("no [opcov] records")
    opcodes = sorted(first_pc)
    encoded = b"".join(opcode.to_bytes(2, "big") for opcode in opcodes)
    return {
        "opcodes": len(opcodes),
        "high_byte_groups": len({opcode >> 8 for opcode in opcodes}),
        "set_sha256": hashlib.sha256(encoded).hexdigest(),
        "first_pc": first_pc,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--require-rom4-idle", action="store_true")
    args = parser.parse_args()
    try:
        result = summarize(args.log.read_text(errors="replace"))
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
