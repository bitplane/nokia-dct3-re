#!/usr/bin/env python3
"""Verify a sibling ROM completes physical mobile-originated SMS."""

import hashlib
import pathlib
import re
import sys

try:
    from tools.radio_outgoing_sms_trace_check import MESSAGE_SENT_HASHES
except ModuleNotFoundError:  # Direct execution from tools/.
    from radio_outgoing_sms_trace_check import MESSAGE_SENT_HASHES


CHECKPOINTS = (
    ("SMS CM Service Request", re.compile(
        r"GSM service establish sapi=0 pd=05 message=24 length=16")),
    ("SAPI 3 SABM", re.compile(r"TX packet type=1b .*data=00800d3f01")),
    ("decoded SMS-SUBMIT", re.compile(
        r"gsm_sms_submit: cp=29 rp=01 smsc=1234567890 "
        r"destination=5551234 alphabet=0 user_length=[1-9][0-9]* outcome=0 "
        r"status_report=0")),
    ("network CP-ACK", re.compile(
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2")),
    ("network RP-ACK", re.compile(
        r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5")),
    ("handset final CP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904")),
    ("RR Channel Release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=2")),
)


def verify(text: str, frame_directory: pathlib.Path) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order product SMS checkpoint: {label}")
        cursor = match.end()
    hashes = {
        hashlib.sha256(frame.read_bytes()).hexdigest()
        for frame in frame_directory.glob("nokia_dct3_lcdmirror_*.pgm")
    }
    if not hashes.intersection(MESSAGE_SENT_HASHES):
        raise ValueError("firmware Message sent frame was not observed")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_product_trace_check.py LOG FRAME_DIR")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - sibling ROM completed physical mobile-originated SMS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
