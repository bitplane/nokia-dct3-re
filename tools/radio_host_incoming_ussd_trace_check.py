#!/usr/bin/env python3
"""Verify host-originated USSD notification delivery."""

import pathlib
import re
import sys


CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: network registered=1 arfcn=1"),
    re.compile(r"gsm_call_adapter: incoming ussd id=1 result=accepted"),
    re.compile(r"gsm_call_adapter: incoming ussd state id=1 .*phase=queued"),
    re.compile(r"gsm_ss: network_initiated operation=notify"),
    re.compile(r"LAPDm service Channel Release acknowledged"),
    re.compile(r"gsm_call_adapter: incoming ussd state id=1 .*phase=delivered"),
)


def verify(text: str) -> None:
    cursor = 0
    for pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing incoming host-USSD checkpoint: {pattern.pattern}")
        cursor = match.end()


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: radio_host_incoming_ussd_trace_check.py LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host USSD notification completed the firmware/RR lifecycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
