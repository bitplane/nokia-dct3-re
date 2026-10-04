#!/usr/bin/env python3
"""Check physical NHM-3 inbox UI and exact persistent SIM record state."""

import argparse
from hashlib import sha256
from pathlib import Path
import sys
import re

from PIL import Image

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.radio_sms_acceptance_common import (
    FIRST_SMS_DELIVER_BODY, require_ordered, require_single_transport, sms_record,
)

READ_FRAME = "ab21e640456a297698ff12e89d315fb469eca215975b8ba4cc5a1a9cb2a41be3"
DELETED_FRAME = "3f5eadd9c60568a7eea484c60e6458682adf7e2510fe387777d119359fc782b6"
SENT_FRAME = "67f74edfd9817c67b2301a1118c32a5764da7ed54e5b1ec09caf9eb332abc7c8"


def verify_sent(log, pixels, size):
    checkpoints = (
        ("physical recipient confirmation", r"6250_sms_input: step=25 pressed=1"),
        ("SMS service request", r"GSM service establish sapi=0 pd=05 message=24 length=16 data=052474"),
        ("exact Hi SMS-SUBMIT",
         r"GSM service uplink sapi=3 pd=09 message=01 length=28 "
         r"data=390119000100069121436587090e11010781551532f40000ff02c834"),
        ("decoded recipient", r"gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2"),
        ("network CP-ACK", r"GSM service downlink kind=17 sapi=3 pd=09 message=04"),
        ("network RP-ACK", r"GSM service downlink kind=18 sapi=3 pd=09 message=01"),
        ("final handset CP-ACK", r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904"),
        ("channel release", r"LAPDm service Channel Release acknowledged nr=2"),
    )
    require_ordered(log, tuple((name, re.compile(pattern)) for name, pattern in checkpoints))
    if log.count("gsm_sms_submit:") != 1:
        raise ValueError("expected exactly one outgoing SMS-SUBMIT")
    if size != (96, 60) or sha256(pixels).hexdigest() != SENT_FRAME:
        raise ValueError("missing reviewed Message sent frame")


def verify(log, nvram, pixels, size, deleted=False):
    require_single_transport(log)
    record = sms_record(nvram)
    expected_status = 0 if deleted else 1
    if record[0] != expected_status or not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
        raise ValueError("expected exact hello payload with selected storage status")
    if log.count("sim_device: update fid=6f3c record=1 length=176") != (3 if deleted else 2):
        raise ValueError("unexpected delivery/read/delete storage-update count")
    expected_frame = DELETED_FRAME if deleted else READ_FRAME
    if size != (96, 60) or sha256(pixels).hexdigest() != expected_frame:
        raise ValueError("missing reviewed inbox outcome frame")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("nvram", type=Path)
    parser.add_argument("frame", type=Path)
    parser.add_argument("--deleted", action="store_true")
    parser.add_argument("--sent", action="store_true")
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as frame:
            if args.sent:
                verify_sent(args.log.read_text(), frame.convert("L").tobytes(), frame.size)
            else:
                verify(args.log.read_text(), args.nvram.read_bytes(),
                       frame.convert("L").tobytes(), frame.size, args.deleted)
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: NHM-3 physical SMS outcome and protocol/storage evidence")


if __name__ == "__main__":
    main()
