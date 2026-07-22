#!/usr/bin/env python3
"""Validate the observed MAD2 reset/peripheral-clock/watchdog boot contract."""

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


def check(events, require_software_reset=False, require_watchdog_reset=False,
          require_watchdog=True, require_clock_lifecycle=True):
    errors = []
    reset_reads = [e for e in events if e["event"] == "R" and e["offset"] == 0x01]
    reset_writes = [e for e in events if e["event"] == "W" and e["offset"] == 0x01]
    clock_writes = [e["data"] for e in events if e["event"] == "W" and e["offset"] == 0x0D]
    watchdog_writes = [e for e in events if e["event"] == "W" and e["offset"] == 0x03]
    timer1_accesses = [e for e in events if 0x04 <= e["offset"] <= 0x07]

    if not reset_reads or reset_reads[0]["data"] != 0x01:
        errors.append("power-on reset-control value 0x01 was not observed")
    if any(e["old"] != 0x01 or e["data"] != 0x05 for e in reset_writes):
        errors.append("reset-control write differed from the observed 0x01 -> 0x05 lifecycle transition")
    if require_clock_lifecycle and clock_writes[:2] != [0x0C, 0x2C]:
        errors.append(f"clock-gate boot sequence was {clock_writes[:2]}, expected [12, 44]")
    try:
        sim_clock_on = clock_writes.index(0x2C)
        sim_clock_off = clock_writes.index(0x0C, sim_clock_on + 1)
    except ValueError:
        if not require_clock_lifecycle:
            sim_clock_on = sim_clock_off = None
        else:
            errors.append("SIM clock gate did not complete the observed 0x0c -> 0x2c -> 0x0c lifecycle")
            sim_clock_on = sim_clock_off = None
    if require_watchdog and any(e["data"] != 0x31 for e in watchdog_writes):
        errors.append("MAD2 watchdog service used a value other than 0x31")
    if timer1_accesses:
        errors.append("Timer1 offsets 0x04..0x07 unexpectedly became active in the boot contract")
    reset_completed = False
    if require_software_reset:
        for write in reset_writes:
            if write["data"] & 0x04:
                reset_completed = any(
                    read["data"] == 0x05 and events.index(read) > events.index(write)
                    for read in reset_reads)
                if reset_completed:
                    break
        if not reset_completed:
            errors.append("MCU reset request was not followed by reset-cause value 0x05")
    watchdog_reset_completed = False
    if require_watchdog_reset:
        for write in watchdog_writes:
            if write["data"] == 0x01:
                watchdog_reset_completed = any(
                    read["data"] == 0x03 and events.index(read) > events.index(write)
                    for read in reset_reads)
                if watchdog_reset_completed:
                    break
        if not watchdog_reset_completed:
            errors.append("MAD2 watchdog expiry was not followed by reset-cause value 0x03")

    return errors, {
        "reset_reads": len(reset_reads),
        "reset_writes": len(reset_writes),
        "clock_writes": len(clock_writes),
        "sim_clock_lifecycle": sim_clock_on is not None and sim_clock_off is not None,
        "watchdog_writes": len(watchdog_writes),
        "timer1_accesses": len(timer1_accesses),
        "software_reset_completed": reset_completed,
        "watchdog_reset_completed": watchdog_reset_completed,
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--require-software-reset", action="store_true")
    parser.add_argument("--require-watchdog-reset", action="store_true")
    parser.add_argument("--allow-no-watchdog", action="store_true")
    parser.add_argument("--allow-incomplete-clock-lifecycle", action="store_true")
    args = parser.parse_args(argv)
    errors, counts = check(
        parse(args.log.read_text(errors="replace")),
        require_software_reset=args.require_software_reset,
        require_watchdog_reset=args.require_watchdog_reset,
        require_watchdog=not args.allow_no_watchdog,
        require_clock_lifecycle=not args.allow_incomplete_clock_lifecycle,
    )
    print(f"MAD2 clock contract: {counts}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
