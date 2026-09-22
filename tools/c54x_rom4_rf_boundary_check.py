#!/usr/bin/env python3
"""Validate the quantified ROM4 receiver-activation boundary."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


SUMMARY_RE = re.compile(
    r"rom4_interface_summary: .*?frame_expiries=(\d+) .*?"
    r"rf_reads=(\d+) rf_port32_writes=(\d+) "
    r"rf_port38_reads=(\d+) rf_port39_reads=(\d+) .*?"
    r"ifr=([0-9a-fA-F]{4}) imr=([0-9a-fA-F]{4})"
)


def check(text: str, minimum_frames: int = 6000) -> dict[str, int]:
    matches = list(SUMMARY_RE.finditer(text))
    if not matches:
        raise ValueError("missing rom4_interface_summary")
    match = matches[-1]
    result = {
        "frame_expiries": int(match.group(1)),
        "rf_reads": int(match.group(2)),
        "rf_port32_writes": int(match.group(3)),
        "rf_port38_reads": int(match.group(4)),
        "rf_port39_reads": int(match.group(5)),
        "ifr": int(match.group(6), 16),
        "imr": int(match.group(7), 16),
    }
    if result["frame_expiries"] < minimum_frames:
        raise ValueError(
            f"only {result['frame_expiries']} frame expiries; expected at least {minimum_frames}"
        )
    if result["rf_reads"] < 1000:
        raise ValueError(f"only {result['rf_reads']} RF reads; receiver did not become active")
    expected_reads = 32 * (result["frame_expiries"] - 21)
    if result["rf_reads"] != expected_reads:
        raise ValueError(
            f"RF read cadence changed: {result['rf_reads']} reads, "
            f"expected {expected_reads} for {result['frame_expiries']} frames"
        )
    if result["rf_port32_writes"] != 0:
        raise ValueError("receiver activation unexpectedly wrote RF port 0x32")
    if result["rf_port38_reads"] or result["rf_port39_reads"]:
        raise ValueError("ROM4 parallel burst path unexpectedly became active")
    if result["imr"] != 0x035F:
        raise ValueError(f"unexpected terminal IMR {result['imr']:04x}")
    if result["ifr"] & 0x0001:
        raise ValueError(f"INT0 remains pending in terminal IFR {result['ifr']:04x}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--minimum-frames", type=int, default=6000)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"), args.minimum_frames)
    except (OSError, ValueError) as error:
        print(f"ROM4 RF activation boundary rejected: {error}", file=sys.stderr)
        return 1
    print(
        "ROM4 RF activation boundary: PASS "
        f"frames={result['frame_expiries']} ifr={result['ifr']:04x} "
        f"imr={result['imr']:04x} rf_reads={result['rf_reads']} "
        "rf_port32_writes=0 rf_port38_reads=0 rf_port39_reads=0"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
