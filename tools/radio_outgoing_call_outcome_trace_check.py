#!/usr/bin/env python3
"""Verify deterministic network outcomes for an organic outgoing GSM call."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
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
    SPEECH,
    TRAFFIC_ASSIGNMENT,
)


REQUEST = re.compile(r"GSM outgoing request id=(?P<id>\d+) digits=(?P<digits>\d+)")
CM_SERVICE_REJECT = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=05 message=22 length=3"
)
NETWORK_DISCONNECT = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=03 message=25 length=5"
)
HANDSET_RELEASE = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=2d length=2"
)
NETWORK_RELEASE_COMPLETE = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=03 message=2a length=2"
)
STATE_ROUNDTRIP = re.compile(r"state_roundtrip: result=pass")


def _forbid(text: str, patterns: tuple[tuple[str, re.Pattern[str]], ...]) -> None:
    for description, pattern in patterns:
        if pattern.search(text):
            raise ValueError(f"unexpected {description}")


def _check_request(text: str, expected_number: str) -> None:
    requests = list(REQUEST.finditer(text))
    if len(requests) != 1:
        raise ValueError("outcome must retain exactly one outgoing request")
    if requests[0].group("id") != "1":
        raise ValueError("first outgoing request must have id 1")
    if requests[0].group("digits") != expected_number:
        raise ValueError(
            f"outgoing request retained {requests[0].group('digits')!r}, "
            f"expected {expected_number!r}"
        )


def check(
        text: str,
        outcome: str,
        expected_number: str = "5551234",
        require_state_roundtrip: bool = False) -> None:
    if require_state_roundtrip and not STATE_ROUNDTRIP.search(text):
        raise ValueError("missing successful save-state roundtrip")
    if outcome == "service-reject":
        require_ordered(
            text,
            (
                ("CM Service Request", CM_SERVICE_REQUEST),
                ("CM Service Reject", CM_SERVICE_REJECT),
                ("RR Channel Release", RR_RELEASE),
                ("return to PCH", PCH),
            ),
            "outgoing service rejection",
        )
        _forbid(text, (
            ("CM Service Accept", CM_SERVICE_ACCEPT),
            ("outgoing SETUP", SETUP),
            ("outgoing request", REQUEST),
            ("traffic assignment", TRAFFIC_ASSIGNMENT),
            ("speech frame", SPEECH),
        ))
        return

    common = (
        ("CM Service Request", CM_SERVICE_REQUEST),
        ("CM Service Accept", CM_SERVICE_ACCEPT),
        ("SETUP", SETUP),
        ("saved outgoing request", REQUEST),
        ("Call Proceeding", CALL_PROCEEDING),
    )
    if outcome == "busy":
        require_ordered(
            text,
            (
                *common,
                ("network Disconnect", NETWORK_DISCONNECT),
                ("handset Release", HANDSET_RELEASE),
                ("network Release Complete", NETWORK_RELEASE_COMPLETE),
                ("RR Channel Release", RR_RELEASE),
                ("return to PCH", PCH),
            ),
            "outgoing busy result",
        )
        _forbid(text, (
            ("traffic assignment", TRAFFIC_ASSIGNMENT),
            ("network Alerting", ALERTING),
            ("network Connect", CONNECT),
            ("speech frame", SPEECH),
        ))
    elif outcome == "no-answer":
        require_ordered(
            text,
            (
                *common,
                ("one traffic assignment", TRAFFIC_ASSIGNMENT),
                ("Assignment Complete", ASSIGNMENT_COMPLETE),
                ("network Alerting", ALERTING),
                ("physical End / Disconnect", DISCONNECT),
                ("network Release", RELEASE),
                ("handset Release Complete", RELEASE_COMPLETE),
                ("RR Channel Release", RR_RELEASE),
                ("return to PCH", PCH),
            ),
            "outgoing no-answer result",
        )
        if len(TRAFFIC_ASSIGNMENT.findall(text)) != 1:
            raise ValueError("no-answer call must contain exactly one traffic assignment")
        _forbid(text, (
            ("network Connect", CONNECT),
            ("handset Connect Acknowledge", CONNECT_ACKNOWLEDGE),
        ))
    else:
        raise ValueError(f"unknown outgoing-call outcome {outcome!r}")
    _check_request(text, expected_number)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument(
        "--outcome", required=True,
        choices=("busy", "no-answer", "service-reject"),
    )
    parser.add_argument("--number", default="5551234")
    parser.add_argument("--require-state-roundtrip", action="store_true")
    args = parser.parse_args()
    try:
        check(
            args.log.read_text(errors="replace"),
            args.outcome,
            args.number,
            args.require_state_roundtrip,
        )
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print(f"OK - organic outgoing call produced clean {args.outcome} outcome")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
