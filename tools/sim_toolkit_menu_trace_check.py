#!/usr/bin/env python3
"""Check organic GSM 11.14 SET UP MENU and menu-selection ENVELOPE."""

import argparse
import hashlib
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


TOP_LEVEL_FRAME = "3269050ee7961f449185e41963bd2affaec0fdbef236e348da808201d1aebb3b"
ITEM_LIST_FRAME = "68972b6a5e71db603318f4830ba23128d385b196f49cbe6795594d9ece3475fb"


def verify(log: str, frames: list[Path]) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "terminal-response data=8103032300020282810301000d03043432",
        "proactive SET UP MENU ready",
        "SIM status ins=14 sw=9128",
        "header cla=a0 ins=12 p1=00 p2=00 p3=28",
        "terminal-response data=810304250002028281030100",
        "SIM status ins=14 sw=9000",
        "header cla=a0 ins=c2 p1=00 p2=00 p3=09",
        "envelope data=d30702020181100101",
        "menu selection item=1 accepted",
        "SIM status ins=c2 sw=9000",
    ])
    hashes = {hashlib.sha256(path.read_bytes()).hexdigest() for path in frames}
    if TOP_LEVEL_FRAME not in hashes:
        raise ValueError("top-level DCT3 menu entry was not captured")
    if ITEM_LIST_FRAME not in hashes:
        raise ValueError("SIM menu item list was not captured")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("frames", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"),
               sorted(args.frames.glob("*.pgm")))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT MENU: FAIL: {error}") from error
    print("SIM TOOLKIT MENU: PASS setup, organic selection envelope and response")


if __name__ == "__main__":
    main()
