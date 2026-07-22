#!/usr/bin/env python3
"""Validate steady-state CCONT watchdog service over a long boot run."""

import argparse
import pathlib
import re
import sys


RELOAD_RE = re.compile(r"ccont_watchdog_service: mask=03 .*? t=([0-9.]+)")


def check(log_text: str, summary_text: str):
    times = [float(value) for value in RELOAD_RE.findall(log_text)]
    errors = []
    if not times:
        errors.append("combined MAD2/CCONT watchdog reload was not observed")
    if "watchdog_terminal:" in log_text:
        errors.append("firmware entered a terminal watchdog path")
    if "ccont_watchdog_expired" in log_text:
        errors.append("CCONT watchdog expired")
    if re.search(r"(?m)^soft_resets=0$", summary_text) is None:
        errors.append("run did not retain zero soft resets")
    return errors, times


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("summary", type=pathlib.Path)
    args = parser.parse_args()
    errors, times = check(
        args.log.read_text(errors="replace"),
        args.summary.read_text(errors="replace"),
    )
    print(f"CCONT watchdog contract: reloads={len(times)} first={times[0] if times else 0:.6f} "
          f"last={times[-1] if times else 0:.6f}")
    for error in errors:
        print(f"error: {error}", file=sys.stderr)
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
