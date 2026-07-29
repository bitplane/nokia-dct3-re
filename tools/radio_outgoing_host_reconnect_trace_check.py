#!/usr/bin/env python3
"""Verify reconnect/restore snapshots and stale-epoch rejection."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_outgoing_call_outcome_trace_check import (
    NETWORK_DISCONNECT,
    RR_RELEASE,
)
from tools.radio_outgoing_call_trace_check import PCH


REQUEST_EPOCH_1 = re.compile(
    r"gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1")
STATE_EPOCH_1 = re.compile(
    r"gsm_call_adapter: state id=1 epoch=1 phase=connected")
ALERTING_EPOCH_1 = re.compile(
    r"gsm_call_adapter: state id=1 epoch=1 phase=alerting")
ROUNDTRIP = re.compile(r"state_roundtrip: result=pass")
REQUEST_EPOCH_2 = re.compile(
    r"gsm_call_adapter: request id=1 epoch=2 digits=5551234 clients=1")
STATE_EPOCH_2 = re.compile(
    r"gsm_call_adapter: state id=1 epoch=2 phase=connected")
STALE = re.compile(
    r"gsm_call_adapter: stale epoch=1 current=2 type=termination "
    r"id=1 result=rejected")
ACCEPTED = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=accepted")


def check(text, phase="connected"):
    if phase == "alerting":
        require_ordered(
            text,
            (
                ("initial/reconnected request", REQUEST_EPOCH_1),
                ("alerting state", ALERTING_EPOCH_1),
                ("current termination", ACCEPTED),
                ("network Disconnect", NETWORK_DISCONNECT),
                ("RR release", RR_RELEASE),
                ("return to PCH", PCH),
            ),
            "host alerting reconnect",
        )
        if len(REQUEST_EPOCH_1.findall(text)) < 2:
            raise ValueError("request was not republished on alerting reconnect")
        if len(ALERTING_EPOCH_1.findall(text)) < 2:
            raise ValueError("alerting state was not republished on reconnect")
        return
    require_ordered(
        text,
        (
            ("initial/reconnected request", REQUEST_EPOCH_1),
            ("connected state", STATE_EPOCH_1),
            ("save-state roundtrip", ROUNDTRIP),
            ("restored request", REQUEST_EPOCH_2),
            ("restored connected state", STATE_EPOCH_2),
            ("stale epoch rejection", STALE),
            ("current termination", ACCEPTED),
            ("network Disconnect", NETWORK_DISCONNECT),
            ("RR release", RR_RELEASE),
            ("return to PCH", PCH),
        ),
        "host reconnect/restore",
    )
    if len(REQUEST_EPOCH_1.findall(text)) < 3:
        raise ValueError("request was not republished on both reconnects")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument(
        "--phase", choices=("connected", "alerting"), default="connected"
    )
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"), args.phase)
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print(
        "OK - reconnect/restore resynchronized "
        "and rejected stale transport input"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
