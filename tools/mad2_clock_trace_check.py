#!/usr/bin/env python3
"""Validate the observed MAD2 reset/clock/watchdog boot contract."""

import argparse
import pathlib
import re
import sys


ACCESS_RE = re.compile(
    r"mad2_clock: event=(?P<event>[RW]) off=(?P<offset>[0-9a-f]{2}) "
    r"data=(?P<data>[0-9a-f]{2})(?: old=(?P<old>[0-9a-f]{2}))? "
    r"counter=(?P<counter>[0-9a-f]{4}).*?pc=(?P<pc>[0-9a-f]{8})",
    re.IGNORECASE,
)


def parse(text):
    result = []
    for match in ACCESS_RE.finditer(text):
        item = match.groupdict()
        result.append({
            "event": item["event"].upper(),
            "offset": int(item["offset"], 16),
            "data": int(item["data"], 16),
            "old": int(item["old"], 16) if item["old"] else None,
            "counter": int(item["counter"], 16),
            "pc": int(item["pc"], 16),
        })
    return result


def check(events):
    errors = []
    reset_reads = [e for e in events if e["event"] == "R" and e["offset"] == 0x01]
    reset_writes = [e for e in events if e["event"] == "W" and e["offset"] == 0x01]
    clock_writes = [e["data"] for e in events if e["event"] == "W" and e["offset"] == 0x0D]
    watchdog_writes = [e for e in events if e["event"] == "W" and e["offset"] == 0x03]
    timer1_accesses = [e for e in events if 0x04 <= e["offset"] <= 0x07]

    if not reset_reads or reset_reads[0]["data"] != 0x01:
        errors.append("power-on reset-control value 0x01 was not observed")
    if not any(e["old"] == 0x01 and e["data"] == 0x05 for e in reset_writes):
        errors.append("reset-control bit-2 lifecycle write 0x01 -> 0x05 was not observed")
    if clock_writes[:2] != [0x0C, 0x2C]:
        errors.append(f"clock-control boot sequence was {clock_writes[:2]}, expected [12, 44]")
    if not watchdog_writes or any(e["data"] != 0x31 for e in watchdog_writes):
        errors.append("MAD2 watchdog service writes were absent or not 0x31")
    if timer1_accesses:
        errors.append("Timer1 offsets 0x04..0x07 unexpectedly became active in the boot contract")

    return errors, {
        "reset_reads": len(reset_reads),
        "reset_writes": len(reset_writes),
        "clock_writes": len(clock_writes),
        "watchdog_writes": len(watchdog_writes),
        "timer1_accesses": len(timer1_accesses),
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args(argv)
    errors, counts = check(parse(args.log.read_text(errors="replace")))
    print(f"MAD2 clock contract: {counts}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
