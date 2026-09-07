#!/usr/bin/env python3
"""Check proactive REFRESH across terminal re-profiling."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "proactive REFRESH ready",
        "SIM status ins=c2 sw=910b",
        "header cla=a0 ins=12 p1=00 p2=00 p3=0b",
        "header cla=a0 ins=10 p1=00 p2=00 p3=05",
        "body ins=10 length=5",
        "SIM status ins=10 sw=9000",
        "terminal-response data=810309010002028281030100",
        "SIM status ins=14 sw=9000",
        "proactive DISPLAY TEXT ready",
    ])
    if compact.count("terminal-response data=810309010002028281030100") != 1:
        raise ValueError("REFRESH must receive exactly one accepted terminal response")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT REFRESH: FAIL: {error}") from error
    print("SIM TOOLKIT REFRESH: PASS re-profiled and resumed card application")


if __name__ == "__main__":
    main()
