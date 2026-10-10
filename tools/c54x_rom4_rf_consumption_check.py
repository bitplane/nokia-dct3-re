#!/usr/bin/env python3
"""Measure bounded native RF consumption, not physical ADC cadence."""
import math
import re


PATTERN = re.compile(
    r"rom4_rf_sample_consumption: index=(\d+) t=([\d.]+) "
    r"pc=([0-9a-f]+) counter=([0-9a-f]+)")


def check(text):
    rows = [(int(i), float(t), int(pc, 16), int(counter, 16))
            for i, t, pc, counter in PATTERN.findall(text)]
    if len(rows) != 64 or [row[0] for row in rows] != list(range(1, 65)):
        raise ValueError("expected exactly the first 64 consecutive RF reads")
    times = [row[1] for row in rows]
    if any(not math.isfinite(t) or t < 0 for t in times):
        raise ValueError("invalid RF read timestamp")
    deltas = [b - a for a, b in zip(times, times[1:])]
    if min(deltas) < 0:
        raise ValueError("RF read timestamps regress")
    return dict(reads=len(rows), first_time=times[0], last_time=times[-1],
                minimum_gap_seconds=min(deltas), maximum_gap_seconds=max(deltas),
                span_seconds=times[-1] - times[0],
                program_counters=sorted(set(row[2] for row in rows)),
                frame_counter_values=sorted(set(row[3] for row in rows)),
                evidence_class="native-read-consumption", physical_sample_rate_proven=False)
