#!/usr/bin/env python3
"""Check an organic GSM 11.14 DISPLAY TEXT -> GET INKEY -> GET INPUT sequence."""

import argparse
import hashlib
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


GET_INPUT_FRAME = "86d2c40c4ecf59e2b0516e1b0b4aee695917699a3b560ae7cc1e54b90f87d6f0"


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
        "terminal-response data=8103022200020282810301000d020435",
        "proactive GET INPUT ready",
        "SIM status ins=14 sw=911a",
        "header cla=a0 ins=12 p1=00 p2=00 p3=1a",
        "header cla=a0 ins=14 p1=00 p2=00 p3=11",
        "terminal-response data=8103032300020282810301000d03043432",
        "SIM status ins=14 sw=9000",
    ])
    hashes = {hashlib.sha256(path.read_bytes()).hexdigest() for path in frames}
    if GET_INPUT_FRAME not in hashes:
        raise ValueError("firmware-rendered GET INPUT value was not captured")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("frames", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"),
               sorted(args.frames.glob("*.pgm")))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT GET INPUT: FAIL: {error}") from error
    print("SIM TOOLKIT GET INPUT: PASS organic two-digit input and terminal response")


if __name__ == "__main__":
    main()
