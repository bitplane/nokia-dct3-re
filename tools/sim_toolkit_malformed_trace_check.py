#!/usr/bin/env python3
"""Check rejection of an unknown required proactive-command TLV."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "menu selection item=1 accepted",
        "proactive UNKNOWN REQUIRED TLV ready",
        "SIM status ins=c2 sw=9114",
        "terminal-response data=810310218002028281030132",
        "SIM status ins=14 sw=9000",
    ])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT MALFORMED: FAIL: {error}") from error
    print("SIM TOOLKIT MALFORMED: PASS unknown required TLV rejected")


if __name__ == "__main__":
    main()
