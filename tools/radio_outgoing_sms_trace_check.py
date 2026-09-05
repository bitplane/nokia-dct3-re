#!/usr/bin/env python3
"""Verify one physical mobile-originated SMS transaction."""

import pathlib
import re
import sys
import hashlib


CHECKPOINTS = (
    ("SMS CM Service Request", re.compile(
        r"GSM service establish sapi=0 pd=05 message=24 length=16 "
        r"data=052474")),
    ("SAPI 3 SABM", re.compile(
        r"TX packet type=1b .*data=00800d3f01")),
    ("first SMS-SUBMIT segment", re.compile(
        r"TX packet type=1b .*data=00800d005329011900010006912143658709"
        r"0e110007815515")),
    ("complete reassembled SMS-SUBMIT", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=01 length=28 "
        r"data=290119000100069121436587090e11000781551532f40000a702c824")),
    ("decoded SMS-SUBMIT", re.compile(
        r"gsm_sms_submit: cp=29 rp=01 smsc=1234567890 "
        r"destination=5551234 alphabet=0 user_length=2")),
    ("network CP-ACK", re.compile(
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2")),
    ("network RP-ACK", re.compile(
        r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5")),
    ("handset final CP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904")),
    ("RR Channel Release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=2")),
)

MESSAGE_SENT_HASHES = {
    "4852028cf4bba7edd0fd45bf7ad8560176e208b5185abfb89a4a156c759d04e4",
    "6061f8c187af3b377518937e5617221fcb67d40cd5c0dcd34f8595b1c6a90dfb",
}


def verify(text: str, frame_directory: pathlib.Path | None = None) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order outgoing-SMS checkpoint: {label}")
        cursor = match.end()

    submits = re.findall(r"gsm_sms_submit:", text)
    if len(submits) != 1:
        raise ValueError(
            f"expected exactly one decoded SMS-SUBMIT, observed {len(submits)}")

    if frame_directory is not None:
        hashes = {
            hashlib.sha256(frame.read_bytes()).hexdigest()
            for frame in frame_directory.glob("nokia_dct3_lcdmirror_*.pgm")
        }
        if not hashes.intersection(MESSAGE_SENT_HASHES):
            raise ValueError("firmware Message sent frame was not observed")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_trace_check.py MAME_ERROR_LOG FRAME_DIR")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - physical SMS-SUBMIT, SAPI 3 reassembly, CP/RP ACK and "
        "dedicated-channel release completed organically")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
