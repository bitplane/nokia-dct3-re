#!/usr/bin/env python3
"""Verify NHM-6 organic incoming-call Answer/End and RR release."""

import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import (
    ALERTING,
    ASSIGNMENT_COMPLETE,
    CIPHER_MODE_COMMAND,
    CIPHER_MODE_COMPLETE,
    CONNECT,
    CONNECT_ACKNOWLEDGE,
    DISCONNECT,
    IDLE_PCH,
    IMSI_PAGE,
    INCOMING_SETUP,
    MM_INFORMATION,
    NETWORK_RELEASE,
    PAGING_CONTENTION_UA,
    PAGING_RESPONSE,
    REGISTRATION_RELEASE,
    RELEASE_COMPLETE,
    RELEASE_CONFIRMATION,
    RR_CHANNEL_RELEASE,
    TRAFFIC_RELEASE_UA,
    TRAFFIC_SABM,
    TRAFFIC_UA,
    require_count,
    require_ordered,
)


CHECKPOINTS = (
    ("registration release", REGISTRATION_RELEASE),
    ("IMSI page", IMSI_PAGE),
    ("Paging Response", PAGING_RESPONSE),
    ("contention-resolution UA", PAGING_CONTENTION_UA),
    ("Cipher Mode Command", CIPHER_MODE_COMMAND),
    ("NHM-6 cipher publication",
     r"TX packet type=14 payload=12"),
    ("MM Information", MM_INFORMATION),
    ("Cipher Mode Complete", CIPHER_MODE_COMPLETE),
    ("incoming SETUP", INCOMING_SETUP),
    ("Call Confirmed",
     r"GSM service uplink sapi=0 pd=03 message=08 length=11"),
    ("Alerting", ALERTING),
    ("traffic configuration",
     r"TX packet type=02 payload=20 .*data=041202000271012fc1"),
    ("traffic SABM", TRAFFIC_SABM),
    ("traffic UA", TRAFFIC_UA),
    ("Assignment Complete", ASSIGNMENT_COMPLETE),
    ("physical Answer", CONNECT),
    ("speech request", r"wire=860b speech_control=060b"),
    ("Connect Acknowledge", CONNECT_ACKNOWLEDGE),
    ("physical End", DISCONNECT),
    ("network Release", NETWORK_RELEASE),
    ("Release Complete", RELEASE_COMPLETE),
    ("RR Channel Release", RR_CHANNEL_RELEASE),
    ("traffic release UA", TRAFFIC_RELEASE_UA),
    ("NHM-6 release transaction",
     r"TX packet type=02 payload=20 .*"
     r"data=041202001117001a600003370000001400000001"),
    ("release confirmation", RELEASE_CONFIRMATION),
    ("speech release", r"wire=840a speech_control=040a"),
    ("idle PCH", IDLE_PCH),
)


def verify(text: str) -> None:
    require_ordered(text, CHECKPOINTS, "NHM-6")
    require_count(text, CONNECT, 1, "NHM-6 must emit exactly one Connect")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_3330_incoming_call_boundary_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print("OK - NHM-6 organically answered, ended and returned to idle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
