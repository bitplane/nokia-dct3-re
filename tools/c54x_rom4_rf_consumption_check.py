#!/usr/bin/env python3
"""Measure bounded native RF consumption, not physical ADC cadence."""
import math
import re


PATTERN = re.compile(
    r"rom4_rf_sample_consumption: index=(\d+) t=([\d.]+) "
    r"pc=([0-9a-f]+) counter=([0-9a-f]+)")


def check_execution_scan(text):
    marker = 'rom4_rf_filter_probe: entries=64 phase=instruction_entry'
    if text.count(marker) != 1:
        raise ValueError('missing or duplicate execution-owned filter probe')
    rows = [line.split('rom4_rf_execution_filter: ', 1)[1] for line in text.splitlines()
            if 'rom4_rf_execution_filter: ' in line]
    if len(rows) != 64:
        raise ValueError('expected exactly 64 executed filter input entries')
    owners = (0x330a, 0x3310, 0x3316, 0x331c)
    for index, row in enumerate(rows):
        match = re.fullmatch(r'index=(\d+) pc=([0-9a-f]{4}) opcode=([0-9a-f]{4}) port=([0-9a-f]{4})', row)
        if (not match or int(match[1]) != index + 1 or int(match[2], 16) != owners[index % 4]
                or match[3] != '74d6' or match[4] != '0027'):
            raise ValueError('executed filter input sequence or PORTR encoding differs')
    return dict(entries=64, program_counters=list(owners), source_port=0x27,
                group_size_input_instructions=32, evidence_class='executed-filter-input-instructions',
                physical_sample_rate_proven=False)


def check(text, *, scan=False):
    pattern = PATTERN if not scan else re.compile(
        PATTERN.pattern.replace("sample_consumption", "scan_consumption"))
    rows = [(int(i), float(t), int(pc, 16), int(counter, 16))
            for i, t, pc, counter in pattern.findall(text)]
    if len(rows) != 64 or [row[0] for row in rows] != list(range(1, 65)):
        raise ValueError("expected exactly the first 64 consecutive RF reads")
    if scan and any(not 0x330a <= row[2] <= 0x3326 for row in rows):
        raise ValueError("scan consumption escaped the reviewed filter window")
    if scan and [row[2] for row in rows] != [0x330c, 0x3312, 0x3318, 0x331e] * 16:
        raise ValueError("scan consumption differs from the byte-pinned four-read loop")
    times = [row[1] for row in rows]
    if any(not math.isfinite(t) or t < 0 for t in times):
        raise ValueError("invalid RF read timestamp")
    deltas = [b - a for a, b in zip(times, times[1:])]
    if min(deltas) < 0:
        raise ValueError("RF read timestamps regress")
    return dict(window="scan" if scan else "startup", reads=len(rows), first_time=times[0], last_time=times[-1],
                minimum_gap_seconds=min(deltas), maximum_gap_seconds=max(deltas),
                span_seconds=times[-1] - times[0],
                group_size_words=32,
                group_spans_seconds=[times[31] - times[0], times[63] - times[32]],
                group_start_interval_seconds=times[32] - times[0],
                between_groups_gap_seconds=times[32] - times[31],
                program_counters=sorted(set(row[2] for row in rows)),
                frame_counter_values=sorted(set(row[3] for row in rows)),
                evidence_class="native-read-consumption", physical_sample_rate_proven=False)
