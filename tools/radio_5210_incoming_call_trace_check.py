#!/usr/bin/env python3
"""Verify the organic NSM-5 incoming-call answer and release lifecycle."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

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
    ("NSM-5 cipher-control publication", re.compile(
        r"TX packet type=14 payload=12 .*data=00ebffffffffffffffff0000")),
    ("MM Information", MM_INFORMATION),
    ("Cipher Mode Complete", CIPHER_MODE_COMPLETE),
    ("incoming SETUP", INCOMING_SETUP),
    ("NSM-5 Call Confirmed", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=08 length=5")),
    ("Alerting", ALERTING),
    ("NSM-5 traffic-channel configuration", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=040002000271012fc10000010000000400000000")),
    ("traffic-main-link SABM", TRAFFIC_SABM),
    ("traffic-main-link UA", TRAFFIC_UA),
    ("Assignment Complete", ASSIGNMENT_COMPLETE),
    ("physical Send Connect", CONNECT),
    ("NSM-5 speech request", re.compile(
        r"doorbell .*wire=860b speech_control=060b")),
    ("Connect Acknowledge", CONNECT_ACKNOWLEDGE),
    ("physical End Disconnect", DISCONNECT),
    ("network Release", NETWORK_RELEASE),
    ("Release Complete", RELEASE_COMPLETE),
    ("RR Channel Release", RR_CHANNEL_RELEASE),
    ("traffic-link release UA", TRAFFIC_RELEASE_UA),
    ("NSM-5 release channel transaction", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=040000001117001a600000560000001400000001")),
    ("release channel confirmation", RELEASE_CONFIRMATION),
    ("NSM-5 speech release", re.compile(
        r"doorbell .*wire=840a speech_control=040a")),
    ("idle PCH schedule", IDLE_PCH),
)


def verify(text: str) -> None:
    require_ordered(text, CHECKPOINTS, "NSM-5")
    require_count(
        text, CONNECT, 1,
        "NSM-5 must emit exactly one Connect before acknowledgement")
    require_count(
        text, DISCONNECT, 1,
        "NSM-5 must emit exactly one physical-End Disconnect")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - NSM-5 completed paging, traffic assignment, physical Answer, "
        "speech control, physical End and return to PCH")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
