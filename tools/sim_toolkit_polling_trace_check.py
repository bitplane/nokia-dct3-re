#!/usr/bin/env python3
"""Check proactive POLL INTERVAL followed by POLLING OFF."""

import argparse
import re
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "proactive POLL INTERVAL ready",
        "SIM status ins=c2 sw=910f",
        "terminal-response data=81030a03000202828103010004020105",
        "proactive POLLING OFF ready",
        "SIM status ins=14 sw=910b",
        "terminal-response data=81030b040002028281030100",
        "SIM status ins=14 sw=9000",
    ])
    off = compact.index("terminal-response data=81030b040002028281030100")
    if re.search(r"header cla=a0 ins=f2", compact[off:]):
        raise ValueError("periodic STATUS continued after POLLING OFF")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT POLLING: FAIL: {error}") from error
    print("SIM TOOLKIT POLLING: PASS interval accepted and polling disabled")


if __name__ == "__main__":
    main()
