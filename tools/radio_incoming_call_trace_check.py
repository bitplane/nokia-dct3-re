#!/usr/bin/env python3
"""Verify one organic network-originated call-control attempt."""

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
    ("DSP cipher-control publication", re.compile(
        r"TX packet type=14 payload=12 .*"
        r"data=00f4ffffffffffffffff0000")),
    ("MM Information", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}2905324762704221000000")),
    ("Cipher Mode Complete", re.compile(
        r"GSM service uplink sapi=0 pd=06 message=32 length=2")),
    ("incoming SETUP", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}45030504046002008134015c0581551532f4")),
    ("Call Confirmed", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=08")),
    ("TCH/F Assignment Command", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}21062e094001006301")),
    ("Alerting", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=01")),
    ("organic TCH/F channel configuration", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=041202860271012fc12b0001002b00042b000000")),
    ("TCH/F channel-change confirmation", re.compile(
        r"RX enqueue type=89 payload=8 .*data=0000000000000000")),
    ("new-link SABM", re.compile(
        r"TX packet type=1b payload=25 .*data=00b0013f01")),
    ("new-link UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301")),
    ("Assignment Complete", re.compile(
        r"GSM service uplink sapi=0 pd=06 message=29 length=3")),
    ("handset Disconnect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=25")),
    ("network Release", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
        r"03[0-9a-f]{2}09032d")),
    ("Release Complete", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=2a")),
    ("RR Channel Release", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
        r"03[0-9a-f]{2}0d060d00")),
    ("new-link DISC", re.compile(
        r"TX packet type=1b payload=25 .*data=00b0015301")),
    ("new-link release UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}017301")),
    ("TCH/F deconfiguration", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=041202001117001a600000010000000800000001")),
    ("TCH/F release confirmation", re.compile(
        r"RX enqueue type=89 payload=8 .*data=0000000000000000")),
    ("return to PCH fill", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
        r"1506210001f0")),
)


def verify(text: str) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order incoming-call checkpoint: {label}")
        cursor = match.end()

    pages = re.findall(r"PCH IMSI page transmitted channel=60", text)
    if len(pages) != 1:
        raise ValueError(f"expected exactly one IMSI page, observed {len(pages)}")

    setups = re.findall(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03[0-9a-f]{2}45030504046002008134015c0581551532f4",
        text)
    if len(setups) != 1:
        raise ValueError(
            f"expected exactly one incoming SETUP, observed {len(setups)}")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_incoming_call_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - page, SC=0 DSP cipher control/complete, MM connection, incoming "
        "SETUP, TCH/F assignment, Assignment Complete and bounded call "
        "clearing completed organically")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
