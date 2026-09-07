#!/usr/bin/env python3
"""Verify host-originated USSD notification delivery."""

import pathlib
import re
import argparse


BASE_CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: network registered=1 arfcn=1"),
    re.compile(r"gsm_call_adapter: incoming ussd id=1 result=accepted"),
    re.compile(r"gsm_call_adapter: incoming ussd state id=1 .*phase=queued"),
)
SERVICE_CHECKPOINTS = (
    re.compile(r"gsm_ss: network_initiated operation=notify"),
    re.compile(r"LAPDm service Channel Release acknowledged"),
    re.compile(r"gsm_call_adapter: incoming ussd state id=1 .*phase=delivered"),
)


def verify(text: str, require_restore: bool = False) -> None:
    checkpoints = list(BASE_CHECKPOINTS)
    if require_restore:
        checkpoints.extend((
            re.compile(r"state_roundtrip: result=pass"),
            re.compile(r"gsm_call_adapter: incoming ussd state id=1 epoch=2 phase=queued"),
        ))
    checkpoints.extend(SERVICE_CHECKPOINTS)
    cursor = 0
    for pattern in checkpoints:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing incoming host-USSD checkpoint: {pattern.pattern}")
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
    print("OK - host USSD notification completed the firmware/RR lifecycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
