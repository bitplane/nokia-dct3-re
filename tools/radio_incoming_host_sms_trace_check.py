#!/usr/bin/env python3
"""Check externally originated SMS delivery remains firmware-owned."""

import pathlib
import re
import argparse


BASE_CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: network registered=1 arfcn=1"),
    re.compile(r"gsm_call_adapter: incoming sms id=1 result=accepted"),
    re.compile(r"gsm_call_adapter: incoming sms state id=1 epoch=1 phase=queued"),
)
SERVICE_CHECKPOINTS = (
    re.compile(r"GSM service downlink kind=16 sapi=3 pd=09 message=01"),
    re.compile(r"GSM service uplink sapi=3 pd=09 message=01"),
)


def verify(text: str, require_restore: bool = False) -> None:
    checkpoints = list(BASE_CHECKPOINTS)
    if require_restore:
        checkpoints.extend((
            re.compile(r"state_roundtrip: result=pass"),
            re.compile(r"gsm_call_adapter: incoming sms state id=1 epoch=2 phase=queued"),
        ))
    checkpoints.extend(SERVICE_CHECKPOINTS)
    checkpoints.append(re.compile(
        rf"gsm_call_adapter: incoming sms state id=1 epoch={'2' if require_restore else '1'} phase=delivered"
    ))
    cursor = 0
    for pattern in checkpoints:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing incoming host-SMS checkpoint: {pattern.pattern}")
        cursor = match.end()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--require-restore", action="store_true")
    args = parser.parse_args()
    try:
        verify(args.log.read_text(), args.require_restore)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host-originated SMS completed the firmware CP/RP lifecycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
