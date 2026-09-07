#!/usr/bin/env python3
"""Check cross-ROM DISPLAY TEXT transactions without model-specific frames."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str, outcome: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    common = [
        "read-binary fid=6fae offset=0 length=1 first=03",
        "header cla=a0 ins=10 p1=00 p2=00 p3=05",
        "SIM status ins=10 sw=9000",
        "proactive DISPLAY TEXT ready",
        "header cla=a0 ins=12 p1=00 p2=00 p3=16",
    ]
    result = {
        "success": "terminal-response data=810301218002028281030100",
        "screen-busy": "terminal-response data=81030121800202828103022001",
    }[outcome]
    require_in_order(compact, common + [result, "SIM status ins=14 sw=9000"])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--outcome", choices=("success", "screen-busy"), required=True)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.outcome)
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT CROSS-ROM: FAIL: {error}") from error
    print(f"SIM TOOLKIT CROSS-ROM: PASS {args.outcome}")


if __name__ == "__main__":
    main()
