#!/usr/bin/env python3
"""Check externally originated SMS delivery remains firmware-owned."""

import pathlib
import re
import sys


CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: network registered=1 arfcn=1"),
    re.compile(r"gsm_call_adapter: incoming sms id=1 result=accepted"),
    re.compile(r"gsm_call_adapter: incoming sms state id=1 epoch=1 phase=queued"),
    re.compile(r"GSM service downlink kind=16 sapi=3 pd=09 message=01"),
    re.compile(r"GSM service uplink sapi=3 pd=09 message=01"),
    re.compile(r"gsm_call_adapter: incoming sms state id=1 epoch=1 phase=delivered"),
)


def verify(text: str) -> None:
    cursor = 0
    for pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing incoming host-SMS checkpoint: {pattern.pattern}")
        cursor = match.end()


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: radio_incoming_host_sms_trace_check.py LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host-originated SMS completed the firmware CP/RP lifecycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
