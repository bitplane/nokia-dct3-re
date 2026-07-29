#!/usr/bin/env python3
"""Verify correlated host termination uses the ordinary GSM clear lifecycle."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_outgoing_call_outcome_trace_check import (
    HANDSET_RELEASE,
    NETWORK_DISCONNECT,
    NETWORK_RELEASE_COMPLETE,
    REQUEST,
    RR_RELEASE,
)
from tools.radio_outgoing_call_trace_check import (
    ALERTING,
    CONNECT,
    CONNECT_ACKNOWLEDGE,
    PCH,
)


CONNECT_DECISION = re.compile(
    r"gsm_call_adapter: decision id=1 outcome=0 result=accepted")
NO_ANSWER_DECISION = re.compile(
    r"gsm_call_adapter: decision id=1 outcome=2 result=accepted")
WRONG_TERMINATION = re.compile(
    r"gsm_call_adapter: termination id=2 cause=16 result=rejected"
)
ACCEPTED_TERMINATION = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=accepted"
)
DUPLICATE_TERMINATION = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=rejected"
)
CONSUMED_TERMINATION = re.compile(
    r"gsm_session: outgoing termination consumed id=1 cause=16"
)
STATE_ROUNDTRIP = re.compile(r"state_roundtrip: result=pass")


def check(
        text: str,
        phase: str = "connected",
        require_state_roundtrip: bool = False) -> None:
    if phase == "connected":
        phase_checkpoints = (
            ("accepted connect decision", CONNECT_DECISION),
            ("network Connect", CONNECT),
            ("handset Connect Acknowledge", CONNECT_ACKNOWLEDGE),
        )
    elif phase == "alerting":
        phase_checkpoints = (
            ("accepted no-answer decision", NO_ANSWER_DECISION),
            ("network Alerting", ALERTING),
        )
    else:
        raise ValueError(f"unknown termination phase {phase!r}")
    require_ordered(
        text,
        (
            ("outgoing request", REQUEST),
            phase_checkpoints[0],
            ("wrong termination rejection", WRONG_TERMINATION),
            ("one accepted termination", ACCEPTED_TERMINATION),
            ("duplicate termination rejection", DUPLICATE_TERMINATION),
            *((("save-state roundtrip", STATE_ROUNDTRIP),)
              if require_state_roundtrip else ()),
            *phase_checkpoints[1:],
            ("consumed termination", CONSUMED_TERMINATION),
            ("network Disconnect", NETWORK_DISCONNECT),
            ("handset Release", HANDSET_RELEASE),
            ("network Release Complete", NETWORK_RELEASE_COMPLETE),
            ("RR Channel Release", RR_RELEASE),
            ("return to PCH", PCH),
        ),
        "host-terminated outgoing call",
    )
    if len(ACCEPTED_TERMINATION.findall(text)) != 1:
        raise ValueError("exactly one correlated termination must be accepted")
    expected_consumptions = 2 if require_state_roundtrip else 1
    if len(CONSUMED_TERMINATION.findall(text)) != expected_consumptions:
        raise ValueError(
            "termination consumption count did not match deterministic replay")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument(
        "--phase", choices=("alerting", "connected"), default="connected")
    parser.add_argument("--require-state-roundtrip", action="store_true")
    args = parser.parse_args()
    try:
        check(
            args.log.read_text(errors="replace"),
            args.phase,
            args.require_state_roundtrip,
        )
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - correlated host termination completed ordinary GSM CC/RR clearing")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
