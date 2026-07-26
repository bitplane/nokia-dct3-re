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
    ("traffic-main-link SABM", re.compile(
        r"TX packet type=1b .*data=00b0013f01")),
    ("traffic-main-link UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301")),
    ("Assignment Complete", re.compile(
        r"GSM service uplink sapi=0 pd=06 message=29 length=3")),
)

CONNECT = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=07 length=2")


def verify(text: str, answered: bool = False) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order NHM-5 call-boundary checkpoint: {label}")
        cursor = match.end()

    if answered and not CONNECT.search(text, cursor):
        raise ValueError("missing NHM-5 physical-answer checkpoint: Connect")


def main() -> int:
    if len(sys.argv) not in (2, 3) or (
            len(sys.argv) == 3 and sys.argv[2] != "--answered"):
        raise SystemExit(
            "usage: radio_3310_incoming_call_boundary_check.py "
            "MAME_ERROR_LOG [--answered]")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"),
               answered=len(sys.argv) == 3)
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    suffix = " and physical Answer emitted Connect" if len(sys.argv) == 3 else ""
    print("OK - NHM-5 organically completed traffic assignment" + suffix)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
