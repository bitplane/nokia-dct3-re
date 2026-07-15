#!/usr/bin/env python3
"""Validate the organic MAD2 timer-0/FIQ transaction lifecycle."""

import argparse
import pathlib
import re
import sys


ACCESS_RE = re.compile(
    r"mad2_timer: event=(?P<event>[RW]) off=(?P<offset>[0-9a-f]{2}) "
    r"data=(?P<data>[0-9a-f]{2})(?: old=(?P<old>[0-9a-f]{2}))?.*?t=(?P<time>[0-9.]+)",
    re.IGNORECASE,
)
ASSERT_RE = re.compile(
    r"mad2_timer: event=assert counter=(?P<counter>[0-9a-f]{4}) "
    r"compare=(?P<compare>[0-9a-f]{4}) divider=(?P<divider>[0-9a-f]{2}) "
    r"pending=(?P<pending>[0-9a-f]{3}) mask=(?P<mask>[0-9a-f]{2}) "
    r"ctrl=(?P<ctrl>[0-9a-f]{2}).*?t=(?P<time>[0-9.]+)",
    re.IGNORECASE,
)
ACK_RE = re.compile(
    r"mad2_timer: event=ack mask=(?P<mask>[0-9a-f]{3}) "
    r"pending_before=(?P<before>[0-9a-f]{3}) "
    r"pending_after=(?P<after>[0-9a-f]{3}).*?t=(?P<time>[0-9.]+)",
    re.IGNORECASE,
)


def parse(text):
    events = []
    for line in text.splitlines():
        match = ACCESS_RE.search(line)
        if match:
            item = match.groupdict()
            events.append(
                {
                    "event": item["event"].upper(),
                    "offset": int(item["offset"], 16),
                    "data": int(item["data"], 16),
                    "old": int(item["old"], 16) if item["old"] else None,
                    "time": float(item["time"]),
                }
            )
            continue
        match = ASSERT_RE.search(line)
        if match:
            item = match.groupdict()
            events.append(
                {
                    "event": "assert",
                    "counter": int(item["counter"], 16),
                    "compare": int(item["compare"], 16),
                    "divider": int(item["divider"], 16),
                    "pending": int(item["pending"], 16),
                    "mask": int(item["mask"], 16),
                    "ctrl": int(item["ctrl"], 16),
                    "time": float(item["time"]),
                }
            )
            continue
        match = ACK_RE.search(line)
        if match:
            item = match.groupdict()
            events.append(
                {
                    "event": "ack",
                    "mask": int(item["mask"], 16),
                    "before": int(item["before"], 16),
                    "after": int(item["after"], 16),
                    "time": float(item["time"]),
                }
            )
    return events


def _word_pairs(events, high_offset, low_offset, direction):
    pairs = []
    for first, second in zip(events, events[1:]):
        if (
            first["event"] == direction
            and second["event"] == direction
            and first.get("offset") == high_offset
            and second.get("offset") == low_offset
            and abs(first["time"] - second["time"]) < 0.00001
        ):
            pairs.append((first["data"] << 8) | second["data"])
    return pairs


def check(events, summary_text, expected_line=4):
    errors = []
    bit = 1 << expected_line
    divider_writes = [
        event for event in events
        if event["event"] == "W" and event.get("offset") == 0x0F
    ]
    counters = _word_pairs(events, 0x10, 0x11, "R")
    compares = _word_pairs(events, 0x12, 0x13, "W")
    assertions = [event for event in events if event["event"] == "assert"]
    acknowledgements = [event for event in events if event["event"] == "ack"]

    if not any(event["data"] == 0xF9 for event in divider_writes):
        errors.append("timer divider 0xf9 was not programmed")
    if not counters:
        errors.append("no coherent timer counter MSB/LSB read found")
    if not compares:
        errors.append("no coherent timer compare MSB/LSB write found")
    if counters and compares and not any(((compare - counter) & 0xFFFF) == 2 for counter in counters for compare in compares):
        errors.append("no counter-plus-two compare programming found")
    if not assertions:
        errors.append("no timer compare assertion found")
    if assertions and not all(event["pending"] & bit for event in assertions):
        errors.append(f"timer assertion did not use expected FIQ line {expected_line}")
    if assertions and any(event["pending"] & ~(bit | 0x100) for event in assertions):
        errors.append("timer assertion included an unexpected base FIQ source")

    unmask_writes = [
        event for event in events
        if event["event"] == "W" and event.get("offset") == 0x0A and not (event["data"] & bit)
    ]
    if not unmask_writes:
        errors.append(f"FIQ line {expected_line} was never unmasked")

    clearing_acks = [
        event for event in acknowledgements
        if (event["mask"] & bit) and (event["before"] & bit) and not (event["after"] & bit)
    ]
    if not clearing_acks:
        errors.append(f"FIQ line {expected_line} was not acknowledged and cleared")

    final_match = re.search(r"^final_fiq_status=([0-9A-Fa-f]{2})$", summary_text, re.MULTILINE)
    if not final_match:
        errors.append("boot summary does not record final FIQ status")

    return errors, {
        "events": len(events),
        "counter_pairs": len(counters),
        "compare_pairs": len(compares),
        "assertions": len(assertions),
        "clearing_acks": len(clearing_acks),
        "line": expected_line,
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--summary", type=pathlib.Path, required=True)
    parser.add_argument("--expected-line", type=int, default=4)
    args = parser.parse_args(argv)

    events = parse(args.log.read_text(errors="replace"))
    errors, counts = check(
        events, args.summary.read_text(errors="replace"), args.expected_line
    )
    print(
        "MAD2 timer contract: "
        f"line {counts['line']}, {counts['counter_pairs']} counter pairs, "
        f"{counts['compare_pairs']} compare pairs, {counts['assertions']} assertions, "
        f"{counts['clearing_acks']} clearing acknowledgements"
    )
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
