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
FIRST_RF_READ_RE = re.compile(r"rom4_rf_read: sample=1 pc=[0-9a-fA-F]{4} frame=(\d+)")
RF_WRITE_RE = re.compile(
    r"rom4_rf_port32: sequence=(\d+) port31=([0-9a-fA-F]{4}) "
    r"port32=([0-9a-fA-F]{4}) pc=([0-9a-fA-F]{4})")


def check(text: str, minimum_frames: int = 6000, *, scan_30s: bool = False) -> dict[str, int]:
    matches = list(SUMMARY_RE.finditer(text))
    if not matches:
        raise ValueError("missing rom4_interface_summary")
    first_read = FIRST_RF_READ_RE.search(text)
    if not first_read:
        raise ValueError("missing first rom4_rf_read sample")
    match = matches[-1]
    result = {
        "frame_expiries": int(match.group(1)),
        "rf_reads": int(match.group(2)),
        "rf_port32_writes": int(match.group(3)),
        "rf_port38_reads": int(match.group(4)),
        "rf_port39_reads": int(match.group(5)),
        "ifr": int(match.group(6), 16),
        "imr": int(match.group(7), 16),
        "first_rf_frame": int(first_read.group(1)),
    }
    if result["frame_expiries"] < minimum_frames:
        raise ValueError(
            f"only {result['frame_expiries']} frame expiries; expected at least {minimum_frames}"
        )
    if result["rf_reads"] < 1000:
        raise ValueError(f"only {result['rf_reads']} RF reads; receiver did not become active")
    if result["first_rf_frame"] != 30:
        raise ValueError(f"RF receiver started on frame {result['first_rf_frame']}, expected 30")
    expected_reads = 32 * (result["frame_expiries"] - result["first_rf_frame"] + 1)
    # The fixed-time cutoff may leave two frames in flight after the
    # documented two-cycle XOR #lk,16 cost is applied.
    if result["rf_reads"] not in (expected_reads, expected_reads - 32,
                                  expected_reads - 64):
        raise ValueError(
            f"RF read cadence changed: {result['rf_reads']} reads, "
            f"expected {expected_reads} for {result['frame_expiries']} frames"
        )
    writes = [tuple(int(value, 16 if index else 10) for index, value in enumerate(row))
              for row in RF_WRITE_RE.findall(text)]
    expected_writes = [(1, 0x2A04, 6, 0xA240), (2, 0x2A04, 6, 0xA240),
                       (3, 0x41, 0x40, 0x3712), (4, 0x2813, 0x30, 0x4028)]
    expected_count = 4
    if scan_30s:
        expected_count = 1580
        expected_writes += [(5, 0x2A04, 6, 0xA240), (6, 0x2A04, 6, 0xA240)]
        expected_writes += [(sequence, 0x2727 + sequence - 7, 0x30, 0x4028)
                            for sequence in range(7, 17)]
        if (result["frame_expiries"], result["rf_reads"]) != (20288, 648288):
            raise ValueError("30-second scan frame/read snapshot changed")
        if not re.search(r"space=io direction=write address=000e value=0546 mask=ffff pc=0959\b", text):
            raise ValueError("missing uploaded scan reload")
        if not re.search(r"rom4_speech_state: address=0284 value=007c mask=ffff pc=3d8f\b.*t=6\.", text):
            raise ValueError("missing later search-list selector")
    if result["rf_port32_writes"] != expected_count or writes != expected_writes:
        raise ValueError("fresh-profile RF port sequence changed")
    # FIQ1 TX completion exposes this additional firmware-owned queue item.
    # Require its producer, rather than accepting a bare synthetic port pair.
    for address, value in (("1922", "0041"), ("1923", "0040")):
        if not re.search(rf"rom4_speech_state: address={address} value={value} mask=ffff pc=a1bc\b", text):
            raise ValueError("missing firmware RF queue producer")
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
    parser.add_argument("--scan-30s", action="store_true",
                        help="check the fixed 30-second multi-entry scan snapshot, not acquisition")
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"), args.minimum_frames, scan_30s=args.scan_30s)
    except (OSError, ValueError) as error:
        print(f"ROM4 RF activation boundary rejected: {error}", file=sys.stderr)
        return 1
    print(
        "ROM4 RF activation boundary: PASS "
        f"frames={result['frame_expiries']} ifr={result['ifr']:04x} "
        f"imr={result['imr']:04x} rf_reads={result['rf_reads']} "
        f"rf_port32_writes={result['rf_port32_writes']} rf_port38_reads=0 rf_port39_reads=0"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
