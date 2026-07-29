#!/usr/bin/env python3
"""Verify deterministic save/load across the pending A5 SDCCH transition."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


MARKER_RE = re.compile(
    r"state_replay: phase=(reference|restored) event=(begin|end) t=([0-9.]+)"
)
ROUNDTRIP_RE = re.compile(r"state_roundtrip: result=(\w+)")
RECORD_TOKENS = (
    "GSM service downlink ",
    "GSM service uplink ",
    "gsm_cipher: ",
    "radio_l1: ",
)


def _records(lines: list[str], begin: int, end: int) -> list[str]:
    return [
        line[line.index(token):]
        for line in lines[begin + 1:end]
        for token in RECORD_TOKENS
        if token in line
    ]


def check(text: str) -> list[str]:
    errors: list[str] = []
    if not (match := ROUNDTRIP_RE.search(text)) or match.group(1) != "pass":
        errors.append("missing successful emulator save/load round trip")

    lines = text.splitlines()
    markers = [
        (match.group(1), match.group(2), index)
        for index, line in enumerate(lines)
        if (match := MARKER_RE.search(line))
    ]
    expected = [
        ("reference", "begin"),
        ("reference", "end"),
        ("restored", "begin"),
        ("restored", "end"),
    ]
    if [(phase, event) for phase, event, _ in markers] != expected:
        errors.append("missing or misordered deterministic replay markers")
        return errors

    reference = _records(lines, markers[0][2], markers[1][2])
    restored = _records(lines, markers[2][2], markers[3][2])
    if reference != restored:
        errors.append("restored cipher transition differs from reference")
    if not any("message=32" in record and "uplink" in record
               for record in reference):
        errors.append("replay interval did not contain Cipher Mode Complete")
    if not any("gsm_cipher: event=activated" in record for record in reference):
        errors.append("replay interval did not contain cipher activation")
    if not any("kind=xcch direction=downlink algorithm=1" in record
               for record in reference):
        errors.append("replay interval did not contain encrypted downlink SDCCH")
    restored_end = markers[3][2]
    if not any("PCH no-identity fill" in line
               for line in lines[restored_end + 1:]):
        errors.append("restored call did not release back to PCH")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    errors = check(args.log.read_text(errors="replace"))
    if errors:
        for error in errors:
            print(f"ERROR - {error}")
        return 1
    print(
        "OK - pending Cipher Mode state replayed through identical activation "
        "and clean PCH return"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
