#!/usr/bin/env python3
"""Validate CCONT-watchdog expiry, reset extent, and retained status."""

import argparse
import pathlib
import re
import sys


EXPIRY_RE = re.compile(r"ccont_watchdog_expired: t=([0-9.]+)")
CAUSE_RE = re.compile(r"ccont_power: event=cause_read data=([0-9a-f]{2}) t=([0-9.]+)", re.I)
CLOCK_RE = re.compile(r"mad2_clock: event=W off=0d data=0c .*?t=([0-9.]+)", re.I)


def check(text):
    errors = []
    expiry = EXPIRY_RE.search(text)
    expiry_time = float(expiry.group(1)) if expiry else None
    causes = [(int(data, 16), float(when)) for data, when in CAUSE_RE.findall(text)]
    clocks = [float(when) for when in CLOCK_RE.findall(text)]

    if expiry_time is None:
        errors.append("CCONT watchdog did not expire")
    else:
        pre_causes = [data for data, when in causes if when < expiry_time]
        post_causes = [data for data, when in causes if when > expiry_time]
        if not post_causes:
            errors.append("firmware did not read retained CCONT status after expiry")
        elif not pre_causes:
            errors.append("firmware did not establish CCONT status before expiry")
        elif post_causes[0] != pre_causes[-1]:
            errors.append(
                f"post-expiry CCONT status was {post_causes[0]:02x}, "
                f"expected retained {pre_causes[-1]:02x}"
            )
        if not any(when > expiry_time for when in clocks):
            errors.append("digital-baseband clock initialization did not restart after CCONT expiry")
    if "ccont_power: event=off" in text:
        errors.append("watchdog expiry incorrectly used the commanded rail-off path")

    return errors, {
        "expiry_time": expiry_time,
        "cause_reads": len(causes),
        "first_post_status": next((data for data, when in causes if expiry_time is not None and when > expiry_time), None),
        "clock_restarts": len(clocks),
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args(argv)
    errors, counts = check(args.log.read_text(errors="replace"))
    print(f"CCONT watchdog expiry contract: {counts}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
