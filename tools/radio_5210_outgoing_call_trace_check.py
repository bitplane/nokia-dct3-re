#!/usr/bin/env python3
"""Verify one organic NSM-5 mobile-originated call lifecycle."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_count, require_ordered
from tools.radio_outgoing_call_trace_check import (
    ALERTING,
    ASSIGNMENT_COMPLETE,
    CALL_PROCEEDING,
    CM_SERVICE_ACCEPT,
    CM_SERVICE_REQUEST,
    CONNECT,
    CONNECT_ACKNOWLEDGE,
    DISCONNECT,
    PCH,
    RELEASE,
    RELEASE_COMPLETE,
    RR_RELEASE,
    SETUP,
    TRAFFIC_ASSIGNMENT,
    decode_called_digits,
)


CHECKPOINTS = (
    ("NSM-5 speech request", re.compile(
        r"doorbell .*wire=860b speech_control=060b")),
    ("CM Service Request", CM_SERVICE_REQUEST),
    ("CM Service Accept", CM_SERVICE_ACCEPT),
    ("SETUP", SETUP),
    ("Call Proceeding", CALL_PROCEEDING),
    ("traffic assignment", TRAFFIC_ASSIGNMENT),
    ("NSM-5 traffic-channel configuration", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=040002000271012fc10000010000000400000000")),
    ("Assignment Complete", ASSIGNMENT_COMPLETE),
    ("remote Alerting", ALERTING),
    ("network Connect", CONNECT),
    ("handset Connect Acknowledge", CONNECT_ACKNOWLEDGE),
    ("physical End / Disconnect", DISCONNECT),
    ("network Release", RELEASE),
    ("handset Release Complete", RELEASE_COMPLETE),
    ("RR Channel Release", RR_RELEASE),
    ("NSM-5 speech release", re.compile(
        r"doorbell .*wire=840a speech_control=040a")),
    ("NSM-5 release channel transaction", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=040000001117001a600000560000001400000001")),
    ("release channel confirmation", re.compile(
        r"RX enqueue type=89 payload=8 .*data=0000000000000000")),
    ("return to PCH", PCH),
)


def verify(text: str, expected_number: str = "5551234") -> None:
    require_ordered(text, CHECKPOINTS, "NSM-5 mobile-originated")
    require_count(
        text, TRAFFIC_ASSIGNMENT, 1,
        "NSM-5 mobile-originated call must contain exactly one traffic assignment")

    setup = SETUP.search(text)
    assert setup is not None
    setup_data = bytes.fromhex(setup.group("data"))
    if len(setup_data) != int(setup.group("length")):
        raise ValueError("NSM-5 SETUP trace length does not match its payload")
    number = decode_called_digits(setup_data)
    if number != expected_number:
        raise ValueError(f"NSM-5 SETUP called {number!r}, expected {expected_number!r}")

    require_count(
        text, CONNECT_ACKNOWLEDGE, 1,
        "NSM-5 must acknowledge exactly one outgoing Connect")
    require_count(
        text, DISCONNECT, 1,
        "NSM-5 must emit exactly one physical-End Disconnect")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--number", default="5551234")
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.number)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - NSM-5 physically dialled, connected and released one call "
        "through its product-owned channel and speech-control boundaries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
