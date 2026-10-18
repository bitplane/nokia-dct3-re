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


def check(text: str, minimum_frames: int = 800, *, scan_30s: bool = False) -> dict[str, int]:
    if "rom4_reset_request" in text or re.search(r"TMS320C54x.*illegal", text):
        raise ValueError("reset or DSP fault in RF boundary observation")
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
    # The four-second coherent boot drains two complete descriptor bursts,
    # followed by the firmware's first operational control. The old five-write
    # fingerprint omitted descriptor entries and is not accepted alongside it.
    burst = [(0x2a04, 6, 0xa240), (0x41, 0x40, 0x3712),
             (0x27a2, 0x30, 0x3712), (0x41, 0x20, 0x3712),
             (0x488, 0x10, 0x3712), (0, 0x50, 0x3712)]
    prefix = burst * 2 + [(0x2813, 0x30, 0x4028)]
    expected_writes = [(i, *row) for i, row in enumerate(prefix, 1)]
    expected_count = 13
    if scan_30s:
        # EF_SMSS belongs to DF_TELECOM. Restoring its old incorrect GSM
        # placement alone restores the previous totals; see rom4_dsp_loader.md.
        # This is an exact cold-input regression snapshot, not silicon timing.
        # Independent port-4 completion drains the original five-pair
        # descriptor bursts. This replaces, rather than accepts alongside,
        # the old payload-alias and ring-plus-slot snapshots.
        expected_count = 1537
        # Logging is capped at 32, independently of the uncapped total.
        prefix = burst * 2 + [(0x2813, 0x30, 0x4028)] + burst * 2
        prefix += [(value, 0x30, 0x4028) for value in range(0x2727, 0x272e)]
        expected_writes = [(i, *row) for i, row in enumerate(prefix, 1)]
        if (result["frame_expiries"], result["rf_reads"]) != (20280, 648032):
            raise ValueError(
                "30-second scan frame/read snapshot changed: "
                f"frames={result['frame_expiries']} expected=20280 "
                f"delta={result['frame_expiries'] - 20280:+d}; "
                f"rf_reads={result['rf_reads']} expected=648032 "
                f"delta={result['rf_reads'] - 648032:+d}; "
                f"rf_port32_writes={result['rf_port32_writes']} expected={expected_count} "
                f"delta={result['rf_port32_writes'] - expected_count:+d}"
            )
        if not re.search(r"space=io direction=write address=000e value=0546 mask=ffff pc=0959\b", text):
            raise ValueError("missing uploaded scan reload")
        if not re.search(r"rom4_speech_state: address=0284 value=007c mask=ffff pc=3d8f\b.*t=6\.", text):
            raise ValueError("missing later search-list selector")
    if result["rf_port32_writes"] != expected_count or writes != expected_writes:
        first_difference = next(
            (index for index in range(max(len(writes), len(expected_writes)))
             if writes[index:index + 1] != expected_writes[index:index + 1]), None
        )
        raise ValueError(
            "fresh-profile RF port sequence changed: "
            f"count={result['rf_port32_writes']} expected={expected_count}; "
            f"first_difference={first_difference}; "
            f"observed={writes[first_difference:first_difference + 1] if first_difference is not None else []}; "
            f"expected={expected_writes[first_difference:first_difference + 1] if first_difference is not None else []}"
        )
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
    parser.add_argument("--minimum-frames", type=int, default=800)
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
