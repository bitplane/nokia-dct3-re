#!/usr/bin/env python3
"""Check proactive PLAY TONE and physical cancellation."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "proactive PLAY TONE ready",
        "SIM status ins=c2 sw=911c",
        "dsp_tone: frequency=1750/0 amplitude=65ac active=1/0",
        "dsp_tone: frequency=0/0 amplitude=65ac active=0/0",
        "terminal-response data=81030c200002028281030111",
        "SIM status ins=14 sw=9000",
    ])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT TONE: FAIL: {error}") from error
    print("SIM TOOLKIT TONE: PASS DSP playback and physical cancellation")


if __name__ == "__main__":
    main()
