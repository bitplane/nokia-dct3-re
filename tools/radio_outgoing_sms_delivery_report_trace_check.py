#!/usr/bin/env python3
"""Verify requested SMS delivery-report transport and SIM persistence."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("status-report-requested submit", re.compile(
        r"gsm_sms_submit: cp=29 rp=01 smsc=1234567890 "
        r"destination=5551234 alphabet=0 user_length=2 outcome=0 "
        r"status_report=1")),
    ("submit RP-ACK", re.compile(
        r"GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5")),
    ("first channel release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=2")),
    ("delivery-report page response", re.compile(
        r"GSM service establish sapi=0 pd=06 message=27 length=16")),
    ("correlated status report", re.compile(
        r"gsm_sms_status_report: mr=00 recipient=5551234 status=00 length=37")),
    ("status-report CP-DATA", re.compile(
        r"GSM service downlink kind=16 sapi=3 pd=09 message=01 length=37")),
    ("handset status-report CP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904")),
    ("handset status-report RP-ACK", re.compile(
        r"GSM service uplink sapi=3 pd=09 message=01 length=5 "
        r"data=8901020242")),
    ("network final CP-ACK", re.compile(
        r"GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2")),
    ("final channel release", re.compile(
        r"LAPDm service Channel Release acknowledged nr=3")),
)

SMS_OFFSET = 50 * 32 + 11 + 9 + 16
EXPECTED_RECORD_PREFIX = bytes.fromhex(
    "03 06 91 21 43 65 87 09 02 00 07 81 55 15 32 f4 "
    "62 90 50 10 00 00 00 62 90 50 10 10 00 00 00")


def verify(text: str, card: bytes) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order delivery-report checkpoint: {label}")
        cursor = match.end()
    updates = re.findall(r"sim_device: update fid=6f3c record=1 length=176", text)
    if len(updates) != 2:
        raise ValueError(
            f"expected submit and status-report EF_SMS writes, got {len(updates)}")
    record = card[SMS_OFFSET:SMS_OFFSET + 176]
    if not record.startswith(EXPECTED_RECORD_PREFIX):
        raise ValueError("persisted SMS-STATUS-REPORT prefix mismatch")
    if record[len(EXPECTED_RECORD_PREFIX):] != bytes([0xff]) * (
            176 - len(EXPECTED_RECORD_PREFIX)):
        raise ValueError("unexpected bytes after persisted SMS-STATUS-REPORT")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_delivery_report_trace_check.py "
            "LOG SIM_CARD_NVRAM")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(),
               pathlib.Path(sys.argv[2]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - requested SMS delivery report completed and persisted")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
