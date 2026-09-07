#!/usr/bin/env python3
"""Check proactive SELECT ITEM and the handset-selected item response."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "menu selection item=1 accepted",
        "proactive SELECT ITEM ready",
        "SIM status ins=c2 sw=9122",
        "header cla=a0 ins=12 p1=00 p2=00 p3=22",
        "terminal-response data=810307240002028281030100100102",
        "SIM status ins=14 sw=9000",
    ])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT SELECT ITEM: FAIL: {error}") from error
    print("SIM TOOLKIT SELECT ITEM: PASS physical choice returned item 2")


if __name__ == "__main__":
    main()
