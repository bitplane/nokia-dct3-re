#!/usr/bin/env python3
"""Verify hostile pre-SETUP host input is bounded and cannot own call state."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_outgoing_call_outcome_trace_check import REQUEST


EARLY_REJECT = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=rejected"
)
OVERFLOW = re.compile(r"gsm_call_adapter: queue overflow dropped=(\d+)")
BUSY_ACCEPTED = re.compile(
    r"gsm_call_adapter: decision id=1 outcome=1 result=accepted"
)
NETWORK_DISCONNECT = re.compile(r"gsm_network: downlink CC Disconnect")


def check(text: str) -> None:
    request = REQUEST.search(text)
    rejected = EARLY_REJECT.search(text)
    overflow = OVERFLOW.search(text)
    busy = BUSY_ACCEPTED.search(text)
    if not request:
        raise ValueError("outgoing request was not published")
    if not rejected or rejected.start() >= request.start():
        raise ValueError("pre-SETUP termination was not rejected before request")
    if not overflow or int(overflow.group(1)) == 0:
        raise ValueError("bounded callback queue did not report saturation")
    if not busy or busy.start() <= request.start():
        raise ValueError("valid post-SETUP decision was not independently accepted")
    if NETWORK_DISCONNECT.search(text):
        raise ValueError("rejected pre-SETUP input initiated call clearing")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - hostile pre-SETUP input was bounded and state-neutral")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
