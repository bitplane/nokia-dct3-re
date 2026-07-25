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
    ("MM Information", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03002905324762704221000000")),
    ("incoming SETUP", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"030245030504046002008134015c0581551532f4")),
    ("Call Confirmed", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=08")),
    ("Alerting", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=01")),
    ("handset Disconnect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=25")),
    ("network Release", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"038409032d")),
    ("RR Channel Release", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"03860d060d00")),
    ("Release Complete", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=2a")),
    ("dedicated-channel release acknowledgement", re.compile(
        r"LAPDm service Channel Release acknowledged nr=4")),
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
        r"030245030504046002008134015c0581551532f4",
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
        "OK - page, MM connection, incoming SETUP, Alerting and bounded "
        "call clearing completed organically")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
