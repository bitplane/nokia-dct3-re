#!/usr/bin/env python3
"""Verify NHM-2 organic ringing, physical Answer/End and RR teardown."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("registration release", r"LAPDm Channel Release acknowledged nr=2"),
    ("IMSI page", r"PCH IMSI page transmitted channel=60 fn="),
    ("Cipher Mode Complete",
     r"GSM service uplink sapi=0 pd=06 message=32 length=2"),
    ("Call Confirmed",
     r"GSM service uplink sapi=0 pd=03 message=08 length=5"),
    ("traffic configuration",
     r"TX packet type=02 payload=20 .*"
     r"data=041202000271012fc10000010000000400000000"),
    ("Assignment Complete",
     r"GSM service uplink sapi=0 pd=06 message=29 length=3"),
    ("Alerting", r"GSM service uplink sapi=0 pd=03 message=01 length=2"),
    ("MAD2 buzzer", r"\[:pup\] buzzer: enabled=1 "),
    ("physical Answer", r"GSM service uplink sapi=0 pd=03 message=07 length=2"),
    ("Connect Acknowledge",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
     r"03[0-9a-f]{2}09030f"),
    ("physical End", r"GSM service uplink sapi=0 pd=03 message=25 length=5"),
    ("network Release",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
     r"03[0-9a-f]{2}09032d"),
    ("RR Channel Release",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
     r"03[0-9a-f]{2}0d060d00"),
    ("traffic-link release UA",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301"),
    ("NHM-2 release transaction",
     r"TX packet type=02 payload=20 .*"
     r"data=041202001117001a600000010000001400000001"),
    ("release confirmation",
     r"RX enqueue type=89 payload=8 .*data=0000000000000000"),
    ("idle PCH",
     r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
     r"1506210001f0"),
)

TRAFFIC_CONFIG = re.compile(
    r"TX packet type=02 payload=20 .*data=041202000271012fc1")
CALL_CONFIRMED = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=08 length=5")


def verify(text: str) -> None:
    cursor = 0
    for label, expression in CHECKPOINTS:
        match = re.search(expression, text[cursor:])
        if not match:
            raise ValueError(
                f"missing or out-of-order NHM-2 call checkpoint: {label}")
        cursor += match.end()

    if len(TRAFFIC_CONFIG.findall(text)) != 1:
        raise ValueError(
            "NHM-2 incoming call must issue exactly one traffic assignment")
    if len(CALL_CONFIRMED.findall(text)) != 2:
        raise ValueError(
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
