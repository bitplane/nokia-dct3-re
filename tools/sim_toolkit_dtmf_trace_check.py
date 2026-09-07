#!/usr/bin/env python3
"""Check the 3210 SEND DTMF capability boundary during an active call."""

import argparse
from pathlib import Path

from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "proactive SET UP CALL ready",
        "GSM outgoing request id=1 digits=5551234",
        "GSM service uplink sapi=0 pd=03 message=0f length=2 data=030f",
        "terminal-response data=810306100002028281030100",
        "proactive SEND DTMF ready",
        "SIM status ins=14 sw=910f",
        "terminal-response data=81030d140002028281030131",
        "SIM status ins=14 sw=9000",
        "GSM service uplink sapi=0 pd=03 message=25",
        "GSM service uplink sapi=0 pd=03 message=2a length=2 data=032a",
    ])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT DTMF: FAIL: {error}") from error
    print("SIM TOOLKIT DTMF: PASS active-call command rejected as unsupported")


if __name__ == "__main__":
    main()
