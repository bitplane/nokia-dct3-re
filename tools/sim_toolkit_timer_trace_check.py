#!/usr/bin/env python3
"""Check the 3210 TIMER MANAGEMENT capability boundary."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "menu selection item=1 accepted",
        "proactive TIMER MANAGEMENT ready",
        "SIM status ins=c2 sw=9113",
        "terminal-response data=81030f270002028281030131",
        "SIM status ins=14 sw=9000",
    ])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT TIMER: FAIL: {error}") from error
    print("SIM TOOLKIT TIMER: PASS command rejected as unsupported")


if __name__ == "__main__":
    main()
