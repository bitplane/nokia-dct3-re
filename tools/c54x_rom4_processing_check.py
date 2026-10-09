#!/usr/bin/env python3
"""Check native ROM4 no-cell filtering, not RF acquisition or sample encoding."""

import argparse
from pathlib import Path
import re
import sys

try:
    from tools.c54x_rom4_rf_boundary_check import check as check_rf
except ModuleNotFoundError:
    from c54x_rom4_rf_boundary_check import check as check_rf


def unique_counts(text, prefix):
    rows = re.findall(prefix + r": address=([0-9a-f]{4}) count=(\d+)\b", text)
    result = {}
    for address, count in rows:
        key = int(address, 16)
        if key in result:
            raise ValueError(f"duplicate {prefix} address {address}")
        result[key] = int(count)
    return result


def check(text):
    rf = check_rf(text)
    counts = unique_counts(text, "rom4_comparison_count")
    modes = unique_counts(text, "rom4_dispatch_count")
    frames = modes.get(0x32f4, 0)
    if frames < 1000:
        raise ValueError("missing sustained mode-1 execution")
    for address in (0x3360, 0x3362, 0x336c, 0x336e, 0x2400, 0x2402):
        if counts.get(address) != frames:
            raise ValueError(f"processing/hook count changed at {address:04x}")
    if counts.get(0x3347) != frames - 2 or counts.get(0x3357) != frames - 2:
        raise ValueError("steady reduction branch count changed")
    for address in (0x3382, 0x3385):
        if counts.get(address) != (frames + 1) // 2:
            raise ValueError("comparison cadence changed")
    for address in (0x33a1, 0x33a4, 0x33ac):
        if counts.get(address) != 0:
            raise ValueError(f"unexpected no-cell comparison continuation at {address:04x}")
    inputs = re.findall(r"rom4_comparison_input: reads=(\d+) nonzero=(\d+) reduction_nonzero=(\d+)\b", text)
    if inputs != [(str(rf["rf_reads"]), "0", "0")]:
        raise ValueError("missing or changed unattached input/output census")
    producers = re.findall(r"rom4_reduction_producer: pc=([0-9a-f]{4}) count=(\d+)\b", text)
    if len(producers) != 3 or dict(producers) != {
            "0f15": "16", "3322": str(frames * 8), "3323": str(frames * 8)}:
        raise ValueError("reduction producer ownership/count changed")
    snapshots = re.findall(
        r"rom4_comparison_fetch: .*?address=([0-9a-f]{4}) word=([0-9a-f]{4}) "
        r"pair94=([0-9a-f]{8}) pair96=([0-9a-f]{8}) a=([0-9a-f]{10}) b=([0-9a-f]{10})\b", text)
    for address, opcode, accumulator in (("2400", "fc00", "0000000000"),
                                        ("2402", "fc00", "0000000000"),
                                        ("3385", "fa43", "0000000fa0")):
        selected = [row[1:] for row in snapshots if row[0] == address]
        expected = (opcode, "0003d9de", "00000fa0", accumulator, "0000000000")
        if len(selected) != 4 or any(row != expected for row in selected):
            raise ValueError(f"missing or changed full-width snapshot at {address}")
    return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        frames = check(args.log.read_text(errors="replace"))
    except (OSError, ValueError) as error:
        print(f"ROM4 no-cell processing rejected: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 no-cell processing: PASS mode1_frames={frames} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
