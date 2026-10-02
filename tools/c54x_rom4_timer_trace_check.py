#!/usr/bin/env python3
"""Check observed ROM4 idle compare cadence, not physical CTSI accuracy."""

import argparse
from pathlib import Path
import re
import sys


FRAME = re.compile(r"rom4_frame_timer: expiry=(\d+) enabled=\d+ length=(\d+) .*? t=(\d+\.\d+)")
SLOT = re.compile(r"rom4_slot_timer: expiry=(\d+) delay=(\d+) .*? t=(\d+\.\d+)")
TOLERANCE = 0.000002  # Six-decimal trace rounding plus one quarter-symbol.


def check(text):
    frames = [(int(n), int(length), float(t)) for n, length, t in FRAME.findall(text)]
    slots = [(int(n), int(delay), float(t)) for n, delay, t in SLOT.findall(text)]
    if len(frames) < 32 or len(slots) < 16:
        raise ValueError("need at least 32 frame and 16 slot observations")
    for events, name in ((frames, "frame"), (slots, "slot")):
        if [event[0] for event in events] != list(range(1, len(events) + 1)):
            raise ValueError(f"non-contiguous {name} expiry sequence")
    if any(length != 4999 for _, length, _ in frames):
        raise ValueError("fixture no longer uses the observed 4999 reload")
    period = 5000 * 12 / 13_000_000
    for (_, delay, timestamp) in slots:
        if not 1 <= delay <= 5000:
            raise ValueError("slot delay outside the frame period")
        if min(abs(timestamp - frame[2]) for frame in frames) > TOLERANCE:
            raise ValueError("idle compare does not coincide with a frame wrap")
    for previous, current in zip(slots, slots[1:]):
        if abs(current[2] - previous[2] - period) > TOLERANCE:
            raise ValueError("idle compare cadence differs from one expiry per frame")
    return {"frames": len(frames), "slots": len(slots), "period_seconds": period}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors="replace"))
    except (OSError, ValueError) as error:
        print(f"ROM4 observed timer cadence: FAIL {error}", file=sys.stderr)
        return 1
    print(f"ROM4 observed timer cadence: PASS frames={result['frames']} slots={result['slots']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
