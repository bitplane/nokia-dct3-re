#!/usr/bin/env python3
"""Verify the host-decided USSD transaction remains firmware-owned."""

import argparse
import pathlib
import re
import sys


CHECKPOINTS = (
    re.compile(r"gsm_ss: request=ussd_host id=1 .*dcs=0f packed_length=5"),
    re.compile(r"gsm_call_adapter: ussd request id=1 .*dcs=0f packed_length=5"),
    re.compile(r"gsm_call_adapter: ussd response id=2 outcome=0 result=rejected"),
    re.compile(r"gsm_call_adapter: ussd response id=1 outcome=0 result=accepted"),
    re.compile(r"LAPDm service Channel Release acknowledged"),
)


def verify(text: str, require_restore: bool = False) -> None:
    cursor = 0
    for pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing host-USSD checkpoint: {pattern.pattern}")
        cursor = match.end()
    if require_restore:
        epochs = re.findall(
            r"gsm_call_adapter: ussd request id=1 epoch=(\d+)", text)
        if len(set(epochs)) < 2:
            raise ValueError("host USSD request was not republished after restore")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log")
    parser.add_argument("--require-restore", action="store_true")
    args = parser.parse_args()
    try:
        verify(pathlib.Path(args.log).read_text(), args.require_restore)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host response completed the firmware USSD and RR lifecycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
