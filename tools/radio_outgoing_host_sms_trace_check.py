#!/usr/bin/env python3
"""Check the host-decided outgoing SMS lifecycle."""

import pathlib
import re
import argparse


CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets=2"),
    re.compile(r"gsm_call_adapter: sms decision id=2 outcome=1 result=rejected"),
    re.compile(r"gsm_call_adapter: sms decision id=1 outcome=0 result=accepted"),
    re.compile(r"gsm_call_adapter: sms decision id=1 outcome=0 result=rejected"),
    re.compile(r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5"),
    re.compile(r"gsm_call_adapter:.*sms.*phase=ended|GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904"),
)


def verify(text: str, *, octets: int = 2) -> None:
    if not 0 <= octets <= 140:
        raise ValueError('SMS octet count outside TP-UD range')
    checkpoints = list(CHECKPOINTS)
    checkpoints[0] = re.compile(
        rf"gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets={octets}\b")
    cursor = 0
    for pattern in checkpoints:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing host-SMS checkpoint: {pattern.pattern}")
        cursor = match.end()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=pathlib.Path)
    parser.add_argument('--octets', type=int, default=2)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(), octets=args.octets)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host SMS request, decision and firmware CP/RP lifecycle completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
