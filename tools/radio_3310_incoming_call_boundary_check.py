#!/usr/bin/env python3
"""Verify the evidenced NHM-5 incoming-call frontier through CC Alerting."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("registration release", re.compile(
        r"LAPDm Channel Release acknowledged nr=2")),
    ("IMSI page", re.compile(
        r"PCH IMSI page transmitted channel=60 fn=")),
    ("Paging Response", re.compile(
        r"TX packet type=1b .*data=0080013f410627")),
    ("contention-resolution UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*0173410627")),
    ("no-cipher Cipher Mode Command", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03000d063500")),
    ("NHM-5 cipher-control publication", re.compile(
        r"TX packet type=14 payload=12 .*"
        r"data=001affffffffffffffff0000")),
    ("MM Information", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}2905324762704221000000")),
    ("Cipher Mode Complete", re.compile(
        r"GSM service uplink sapi=0 pd=06 message=32 length=2")),
    ("incoming SETUP", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}45030504046002008134015c0581551532f4")),
    ("Call Confirmed", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=08 length=11")),
    ("Alerting", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=01 length=2")),
    ("traffic-channel configuration", re.compile(
        r"TX packet type=02 payload=20 .*data=041202000271012fc1")),
)

TRAFFIC_SABM = re.compile(
    r"TX packet type=1b .*radio_phase=traffic_lapdm_establish")


def verify(text: str) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order NHM-5 call-boundary checkpoint: {label}")
        cursor = match.end()

    if TRAFFIC_SABM.search(text, cursor):
        raise ValueError(
            "NHM-5 advanced beyond the documented frontier; promote the gate")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_3310_incoming_call_boundary_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print("OK - NHM-5 organically emitted Call Confirmed and Alerting, then requested TCH")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
