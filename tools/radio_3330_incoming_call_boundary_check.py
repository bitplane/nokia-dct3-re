#!/usr/bin/env python3
"""Verify NHM-6 organic incoming-call Answer/End and RR release."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("registration release", r"LAPDm Channel Release acknowledged nr=2"),
    ("IMSI page", r"PCH IMSI page transmitted channel=60 fn="),
    ("Paging Response", r"TX packet type=1b .*data=0080013f410627"),
    ("contention-resolution UA",
     r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*0173410627"),
    ("Cipher Mode Command",
     r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}03000d063500"),
    ("NHM-6 cipher publication",
     r"TX packet type=14 payload=12 .*data=0060ffffffffffffffff0000"),
    ("MM Information",
     r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
     r"03[0-9a-f]{2}2905324762704221000000"),
    ("Cipher Mode Complete",
     r"GSM service uplink sapi=0 pd=06 message=32 length=2"),
    ("incoming SETUP",
     r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
     r"03[0-9a-f]{2}45030504046002008134015c0581551532f4"),
    ("Call Confirmed",
     r"GSM service uplink sapi=0 pd=03 message=08 length=11"),
    ("Alerting", r"GSM service uplink sapi=0 pd=03 message=01 length=2"),
    ("traffic configuration",
     r"TX packet type=02 payload=20 .*data=041202000271012fc1"),
    ("traffic SABM", r"TX packet type=1b .*data=00b0013f01"),
    ("traffic UA",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301"),
    ("Assignment Complete",
     r"GSM service uplink sapi=0 pd=06 message=29 length=3"),
    ("physical Answer", r"GSM service uplink sapi=0 pd=03 message=07 length=2"),
    ("speech request", r"wire=860b speech_control=060b"),
    ("Connect Acknowledge",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
     r"03[0-9a-f]{2}09030f"),
    ("physical End", r"GSM service uplink sapi=0 pd=03 message=25 length=5"),
    ("network Release",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
     r"03[0-9a-f]{2}09032d"),
    ("Release Complete",
     r"GSM service uplink sapi=0 pd=03 message=2a length=2"),
    ("RR Channel Release",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
     r"03[0-9a-f]{2}0d060d00"),
    ("traffic release UA",
     r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301"),
    ("NHM-6 release transaction",
     r"TX packet type=02 payload=20 .*"
     r"data=041202001117001a600003370000001400000001"),
    ("release confirmation",
     r"RX enqueue type=89 payload=8 .*data=0000000000000000"),
    ("speech release", r"wire=840a speech_control=040a"),
    ("idle PCH",
     r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
     r"1506210001f0"),
)


def verify(text: str) -> None:
    cursor = 0
    for label, expression in CHECKPOINTS:
        match = re.search(expression, text[cursor:])
        if not match:
            raise ValueError(
                f"missing or out-of-order NHM-6 call checkpoint: {label}")
        cursor += match.end()

    if len(re.findall(
            r"GSM service uplink sapi=0 pd=03 message=07 length=2",
            text)) != 1:
        raise ValueError("NHM-6 must emit exactly one Connect")


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
