#!/usr/bin/env python3
"""Check that a masked CCONT source remains pending and delivers on unmask."""

import argparse
import re
from pathlib import Path


MASK_RE = re.compile(r"ccont_rtc: event=mask_write data=([0-9a-f]{2}) status=([0-9a-f]{2}) t=([0-9.]+)")
READ_RE = re.compile(r"ccont_rtc: event=read reg=0e data=([0-9a-f]{2}) t=([0-9.]+)")
ROUTE_RE = re.compile(r"ccont_route: state=([01]) irq_line=2 .* t=([0-9.]+)")


def check(text: str):
    errors = []
    masks = [(int(mask, 16), int(status, 16), float(time)) for mask, status, time in MASK_RE.findall(text)]
    held = next(((mask, status, time) for mask, status, time in masks if mask == 0xf8), None)
    released = next(((mask, status, time) for mask, status, time in masks if mask == 0x78 and status & 0x80), None)
    if held is None:
        errors.append("all CCONT interrupt sources were not masked")
    reads = [(int(data, 16), float(time)) for data, time in READ_RE.findall(text)]
    pending = next(((data, time) for data, time in reads if held and time > held[2] and data & 0x80), None)
    if pending is None:
        errors.append("alarm source was not readable while masked")
    if released is None:
        errors.append("pending alarm was not present at unmask")
    routes = [(int(state), float(time)) for state, time in ROUTE_RE.findall(text)]
    if held and any(state and held[2] < time < (released[2] if released else float("inf")) for state, time in routes):
        errors.append("CCONT IRQ asserted while alarm remained masked")
    if released and not any(state and time >= released[2] for state, time in routes):
        errors.append("CCONT IRQ did not assert when pending alarm was unmasked")
    return errors


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    errors = check(args.log.read_text(errors="replace"))
    if errors:
        raise SystemExit("; ".join(errors))
    print("OK - masked CCONT alarm remained pending and asserted IRQ2 on unmask")


if __name__ == "__main__":
    main()
