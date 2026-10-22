#!/usr/bin/env python3
"""Check proactive POLL INTERVAL followed by POLLING OFF."""

import argparse
import re
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.sim_toolkit_trace_check import require_in_order


def verify_commands(log: str) -> None:
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


def verify(log: str) -> None:
    verify_commands(log)
    compact = log.replace("[:sim_card] ", "")
    # Retain the original NSE-8 fixture's measured silence assertion. This
    # is not a universal requirement: GSM 11.14 6.4.14 preserves SIM presence
    # detection, which can still use STATUS after proactive POLLING OFF.
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
