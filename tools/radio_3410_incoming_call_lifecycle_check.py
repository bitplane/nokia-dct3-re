#!/usr/bin/env python3
"""Verify NHM-2 organic ringing, physical Answer/End and RR teardown."""

import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import (
    ALERTING,
    ASSIGNMENT_COMPLETE,
    CIPHER_MODE_COMPLETE,
    CONNECT,
    CONNECT_ACKNOWLEDGE,
    DISCONNECT,
    IDLE_PCH,
    IMSI_PAGE,
    NETWORK_RELEASE,
    REGISTRATION_RELEASE,
    RELEASE_CONFIRMATION,
    RR_CHANNEL_RELEASE,
    TRAFFIC_RELEASE_UA,
    require_count,
    require_ordered,
)


CHECKPOINTS = (
    ("registration release", REGISTRATION_RELEASE),
    ("IMSI page", IMSI_PAGE),
    ("Cipher Mode Complete", CIPHER_MODE_COMPLETE),
    ("Call Confirmed",
     r"GSM service uplink sapi=0 pd=03 message=08 length=5"),
    ("traffic configuration",
     r"TX packet type=02 payload=20 .*"
     r"data=041202000271012fc10000010000000400000000"),
    ("Assignment Complete", ASSIGNMENT_COMPLETE),
    ("Alerting", ALERTING),
    ("MAD2 buzzer", r"\[:pup\] buzzer: enabled=1 "),
    ("physical Answer", CONNECT),
    ("Connect Acknowledge", CONNECT_ACKNOWLEDGE),
    ("physical End", DISCONNECT),
    ("network Release", NETWORK_RELEASE),
    ("RR Channel Release", RR_CHANNEL_RELEASE),
    ("traffic-link release UA", TRAFFIC_RELEASE_UA),
    ("NHM-2 release transaction",
     r"TX packet type=02 payload=20 .*"
     r"data=041202001117001a600000010000001400000001"),
    ("release confirmation", RELEASE_CONFIRMATION),
    ("idle PCH", IDLE_PCH),
)

TRAFFIC_CONFIG = re.compile(
    r"TX packet type=02 payload=20 .*data=041202000271012fc1")
CALL_CONFIRMED = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=08 length=5")


def verify(text: str) -> None:
    require_ordered(text, CHECKPOINTS, "NHM-2")
    require_count(
        text, TRAFFIC_CONFIG, 1,
        "NHM-2 incoming call must issue exactly one traffic assignment")
    require_count(
        text, CALL_CONFIRMED, 2,
        "NHM-2 lifecycle did not retain its observed repeated Call Confirmed")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_3410_incoming_call_lifecycle_check.py "
            "MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print("OK - NHM-2 organically rang, answered, ended and returned to PCH")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
