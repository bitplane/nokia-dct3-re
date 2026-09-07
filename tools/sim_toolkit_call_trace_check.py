#!/usr/bin/env python3
"""Check organic SIM Toolkit SET UP CALL through GSM call control."""

import argparse
from pathlib import Path

from radio_call_lifecycle_common import require_ordered
from radio_outgoing_call_trace_check import (
    ALERTING,
    ASSIGNMENT_COMPLETE,
    CALL_PROCEEDING,
    CONNECT,
    CONNECT_ACKNOWLEDGE,
    DISCONNECT,
    RELEASE,
    RELEASE_COMPLETE,
    RR_RELEASE,
    SETUP,
    TRAFFIC_ASSIGNMENT,
    decode_called_digits,
)
from sim_toolkit_trace_check import require_in_order


def verify(log: str) -> None:
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "envelope data=d30702020181100101",
        "menu selection item=1 accepted",
        "proactive SET UP CALL ready",
        "SIM status ins=c2 sw=911c",
        "header cla=a0 ins=12 p1=00 p2=00 p3=1c",
        "GSM outgoing request id=1 digits=5551234",
        "terminal-response data=810306100002028281030100",
        "SIM status ins=14 sw=9000",
    ])
    require_ordered(log, (
        ("SETUP", SETUP),
        ("Call Proceeding", CALL_PROCEEDING),
        ("traffic assignment", TRAFFIC_ASSIGNMENT),
        ("Assignment Complete", ASSIGNMENT_COMPLETE),
        ("Alerting", ALERTING),
        ("Connect", CONNECT),
        ("Connect Acknowledge", CONNECT_ACKNOWLEDGE),
        ("physical End / Disconnect", DISCONNECT),
        ("network Release", RELEASE),
        ("Release Complete", RELEASE_COMPLETE),
        ("RR Channel Release", RR_RELEASE),
    ), "toolkit call")
    setup = SETUP.search(log)
    assert setup is not None
    if decode_called_digits(bytes.fromhex(setup.group("data"))) != "5551234":
        raise ValueError("toolkit SETUP did not carry destination 5551234")
    if len(TRAFFIC_ASSIGNMENT.findall(log)) != 1:
        raise ValueError("toolkit call must contain exactly one traffic assignment")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT CALL: FAIL: {error}") from error
    print("SIM TOOLKIT CALL: PASS proactive setup completed through CC/RR release")


if __name__ == "__main__":
    main()
