#!/usr/bin/env python3
"""Validate steady-state CCONT watchdog service over a long boot run."""

import argparse
import pathlib
import re


RELOAD_RE = re.compile(r"ccont_watchdog_service: mask=03 .*? t=([0-9.]+)")


def check(log_text: str, summary_text: str):
    times = [float(value) for value in RELOAD_RE.findall(log_text)]
    errors = []
    if len(times) < 10:
        errors.append(f"only {len(times)} combined watchdog reloads were observed")
    if any(later - earlier > 5.0 for earlier, later in zip(times, times[1:])):
        errors.append("combined watchdog reload gap exceeded five seconds")
    if "ccont_watchdog_expired" in log_text:
        errors.append("CCONT watchdog expired")
    if "soft_resets=0" not in summary_text:
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
        print(f"error: {error}")
    return int(bool(errors))


if __name__ == "__main__":
    raise SystemExit(main())
