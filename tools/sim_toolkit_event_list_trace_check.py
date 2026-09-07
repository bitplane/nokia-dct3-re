#!/usr/bin/env python3
"""Check the 3210's explicit SET UP EVENT LIST capability boundary."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "menu selection item=1 accepted",
        "proactive SET UP EVENT LIST ready",
        "SIM status ins=c2 sw=910f",
        "header cla=a0 ins=12 p1=00 p2=00 p3=0f",
        "terminal-response data=810308050002028281030131",
        "SIM status ins=14 sw=9000",
    ])
    if "envelope data=d6" in compact:
        raise ValueError("firmware emitted EVENT DOWNLOAD after rejecting subscription")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT EVENT LIST: FAIL: {error}") from error
    print("SIM TOOLKIT EVENT LIST: PASS 3210 rejected unsupported subscription")


if __name__ == "__main__":
    main()
