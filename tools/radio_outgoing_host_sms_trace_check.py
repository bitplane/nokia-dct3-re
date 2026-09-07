#!/usr/bin/env python3
"""Check the host-decided outgoing SMS lifecycle."""

import pathlib
import re
import sys


CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets=2"),
    re.compile(r"gsm_call_adapter: sms decision id=2 outcome=1 result=rejected"),
    re.compile(r"gsm_call_adapter: sms decision id=1 outcome=0 result=accepted"),
    re.compile(r"gsm_call_adapter: sms decision id=1 outcome=0 result=rejected"),
    re.compile(r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5"),
    re.compile(r"gsm_call_adapter:.*sms.*phase=ended|GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904"),
)


def verify(text: str) -> None:
    cursor = 0
    for pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing host-SMS checkpoint: {pattern.pattern}")
        cursor = match.end()


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: radio_outgoing_host_sms_trace_check.py LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host SMS request, decision and firmware CP/RP lifecycle completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
