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


def check_scan_transitions(text):
    pattern = re.compile(
        r'rom4_processing_transition: address=([0-9a-f]{4}) sequence=(\d+) '
        r'fd=([0-9a-f]{4}) mode=([0-9a-f]{4}) flags=([0-9a-f]{4}) candidate=([0-9a-f]{4}) '
        r'control2180=([0-9a-f]{4}) control21c2=([0-9a-f]{4}) '
        r'a=([0-9a-f]{10}) b=([0-9a-f]{10}) t=([0-9.]+)')
    rows = pattern.findall(text)
    if text.count('rom4_processing_transition:') != len(rows):
        raise ValueError('malformed processing transition')
    early = [row for row in rows if row[0] == '332b']
    calls = [row for row in rows if row[0] == '33ac']
    if len(rows) != 22 or len(early) != 20 or len(calls) != 2:
        raise ValueError('incomplete multi-entry transition capture')
    for index, row in enumerate(early, 1):
        flags = ['0838', '083a'][index - 1] if index <= 2 else ('0828' if index % 2 else '082a')
        candidate = {1: '0027', 2: '002d'}.get(index, '005a')
        if row[1:10] != (str(index), '0000', '0001', flags, candidate,
                         '005a', 'f4cf', '0000000000', '0000000000'):
            raise ValueError('early reduction selector/state changed')
    for index, row in enumerate(calls, 1):
        if row[1:10] != (str(index), '0000' if index == 1 else '0001', '0001',
                         '083a' if index == 1 else '0832', '0027', '005a', 'f4cf',
                         '00002d0000', '00005a0000'):
            raise ValueError('control continuation operands changed')
    writes = re.findall(r'rom4_comparison_write: t=([0-9.]+) pc=([0-9a-f]{4}) address=06fd data=([0-9a-f]{4})', text)
    if [row[1:] for row in writes] != [('0f10', '0000'), ('312c', '0000'), ('3135', '0001')]:
        raise ValueError('reduction selector writer lifecycle changed')
    times = [float(row[10]) for row in rows]
    if times != sorted(times) or not (float(writes[0][0]) < float(early[0][10])
            and float(early[1][10]) < float(writes[1][0]) < float(early[2][10])
            and float(calls[0][10]) < float(early[1][10])
            and float(early[-1][10]) < float(writes[-1][0]) < float(calls[1][10])):
        raise ValueError('reduction/control continuation ordering changed')


def check(text, *, scan_30s=False):
    rf = check_rf(text, scan_30s=scan_30s)
    counts = unique_counts(text, "rom4_comparison_count")
    modes = unique_counts(text, "rom4_dispatch_count")
    frames = modes.get(0x32f4, 0)
    if frames < 1000:
        raise ValueError("missing sustained mode-1 execution")
    if scan_30s and frames != 1024:
        raise ValueError('multi-entry mode-1 snapshot changed')
    if counts.get(0x330a) != frames * 8:
        raise ValueError("filter loop count changed")
    filter_entries = re.findall(
        r"rom4_filter_entry: st1=([0-9a-f]{4}) bk=([0-9a-f]{4}) "
        r"ar0=([0-9a-f]{4}) ar3=([0-9a-f]{4}) ar4=([0-9a-f]{4}) "
        r"ar5=([0-9a-f]{4}) ar6=([0-9a-f]{4}) coefficients=([^\s]+)", text)
    coefficients = "0387,11a3,2ad6,4000,4000,2ad6,11a3,0387"
    expected_filter_entries = [
        ("c900" if scan_30s else "e900", "0010", "0002", output, history, "21a9", input_history, coefficients)
        for output, history, input_history in (
            ("1a48", "0084", "0080"), ("1a4a", "0088", "0084"),
            ("1a4c", "008c", "0088"), ("1a4e", "0080", "008c"))]
    if filter_entries != expected_filter_entries:
        raise ValueError("missing or changed filter entry state/coefficients")
    for address in (0x3360, 0x3362, 0x336c, 0x336e, 0x2400, 0x2402):
        if counts.get(address) != frames:
            raise ValueError(f"processing/hook count changed at {address:04x}")
    early_frames = 20 if scan_30s else 2
    if counts.get(0x3347) != frames - early_frames or counts.get(0x3357) != frames - early_frames:
        raise ValueError("steady reduction branch count changed")
    if scan_30s and counts.get(0x332b) != early_frames:
        raise ValueError('early reduction branch count changed')
    for address in (0x3382, 0x3385):
        if counts.get(address) != (frames + 1) // 2:
            raise ValueError("comparison cadence changed")
    for address in (0x33a1, 0x33a4):
        if counts.get(address) != 0:
            raise ValueError(f"unexpected no-cell comparison continuation at {address:04x}")
    if counts.get(0x33ac) != (2 if scan_30s else 0):
        raise ValueError('unexpected control continuation count')
    if scan_30s:
        check_scan_transitions(text)
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
    parser.add_argument('--scan-30s', action='store_true', help='require the current multi-entry cold scan profile')
    args = parser.parse_args()
    try:
        frames = check(args.log.read_text(errors="replace"), scan_30s=args.scan_30s)
    except (OSError, ValueError) as error:
        print(f"ROM4 no-cell processing rejected: {error}", file=sys.stderr)
        return 1
    print(f"ROM4 no-cell processing: PASS mode1_frames={frames} acquisition_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
