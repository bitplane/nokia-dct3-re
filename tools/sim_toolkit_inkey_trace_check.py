#!/usr/bin/env python3
"""Check an organic GSM 11.14 DISPLAY TEXT -> GET INKEY sequence."""

import argparse
import hashlib
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


GET_INKEY_FRAME = "72be02a2b0860350bb0f5365ee4add634dac47c25a21e95ff4f60d34505017f2"


def verify(log: str, frames: list[Path]) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "header cla=a0 ins=10 p1=00 p2=00 p3=05",
        "proactive DISPLAY TEXT ready",
        "header cla=a0 ins=12 p1=00 p2=00 p3=16",
        "terminal-response data=810301218002028281030100",
        "proactive GET INKEY ready",
        "SIM status ins=14 sw=9115",
        "header cla=a0 ins=12 p1=00 p2=00 p3=15",
        "header cla=a0 ins=14 p1=00 p2=00 p3=10",
        "terminal-response data=8103022200020282810301000d020435",
        "SIM status ins=14 sw=9000",
    ])
    hashes = {hashlib.sha256(path.read_bytes()).hexdigest() for path in frames}
    if GET_INKEY_FRAME not in hashes:
        raise ValueError("firmware-rendered GET INKEY prompt was not captured")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("frames", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"),
               sorted(args.frames.glob("*.pgm")))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT GET INKEY: FAIL: {error}") from error
    print("SIM TOOLKIT GET INKEY: PASS organic digit and terminal response")


if __name__ == "__main__":
    main()
