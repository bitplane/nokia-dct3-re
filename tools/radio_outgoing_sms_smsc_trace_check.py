#!/usr/bin/env python3
"""Verify an organic EF_SMSP edit controls a later mobile-originated SMS."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("EF_SMSP update", re.compile(
        r"sim_device: update fid=6f42 record=1 length=44")),
    ("SMS CM Service Request", re.compile(
        r"GSM service establish sapi=0 pd=05 message=24 length=16 "
        r"data=052474")),
    ("decoded SMS-SUBMIT", re.compile(
        r"gsm_sms_submit: cp=29 rp=01 smsc=9876543210 "
        r"destination=5551234 alphabet=0 user_length=2 outcome=0")),
    ("network CP-ACK", re.compile(
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2")),
    ("network RP-ACK", re.compile(
        r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5")),
    ("handset final CP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904")),
    ("RR Channel Release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=2")),
)

SMSP_OFFSET = 50 * 32 + 11 + 9 + 16 + 10 * 176
EXPECTED_FIRST_RECORD = bytes(
    [0xff] * 16 + [0xfd] + [0xff] * 12 +
    [0x06, 0x81, 0x89, 0x67, 0x45, 0x23, 0x01] + [0xff] * 8)


def verify(text: str, card: bytes) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order SMSC checkpoint: {label}")
        cursor = match.end()

    actual = card[SMSP_OFFSET:SMSP_OFFSET + len(EXPECTED_FIRST_RECORD)]
    if actual != EXPECTED_FIRST_RECORD:
        raise ValueError(
            "EF_SMSP record 1 mismatch: " + actual.hex(" "))


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_smsc_trace_check.py "
            "MAME_ERROR_LOG SIM_CARD_NVRAM")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(),
               pathlib.Path(sys.argv[2]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - firmware persisted EF_SMSP and used its edited service centre "
        "for an acknowledged SMS-SUBMIT")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
