#!/usr/bin/env python3
"""Check pre-page malformed rejection and post-ACK duplicate suppression."""

import pathlib
import re
import sys
import hashlib

SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
SMS_RECORD_SIZE = 176
SMS_DELIVER_BODY = bytes.fromhex(
    "06912143658709040781551532f400006270422100000005e8329bfd06")


def _record(data: bytes, number: int) -> bytes:
    start = SMS_NVRAM_OFFSET + (number - 1) * SMS_RECORD_SIZE
    return data[start:start + SMS_RECORD_SIZE]


CAPACITY_UI_SHA256 = (
    "faa136d99b03e0aebbbb016a5b6c40e55af3f6fb2872c7a33f915cc3acb6f067")


def verify(
        log: str,
        sim_nvram: bytes,
        outcome: str,
        snapshots: pathlib.Path | None = None) -> None:
    pages = len(re.findall(r"PCH IMSI page transmitted channel=60", log))
    writes = len(re.findall(r"sim_device: update fid=6f3c", log))
    if outcome == "malformed":
        if len(re.findall(
                r"GSM incoming service rejected before paging service=2",
                log)) != 1:
            raise ValueError("malformed SMS was not rejected exactly once")
        if pages or writes:
            raise ValueError("malformed SMS reached paging or SIM storage")
        for number in range(1, 11):
            if _record(sim_nvram, number)[0] != 0x00:
                raise ValueError("malformed SMS mutated an EF_SMS record")
    elif outcome == "duplicate":
        if pages != 1 or writes != 1:
            raise ValueError("duplicate SMS was paged or stored more than once")
        if "duplicate queued rp_reference=40 suppressed" not in log:
            raise ValueError("missing correlated duplicate suppression")
        first = _record(sim_nvram, 1)
        if first[0] != 0x03 or not first[1:].startswith(SMS_DELIVER_BODY):
            raise ValueError("the original SMS was not retained exactly")
        if _record(sim_nvram, 2)[0] != 0x00:
            raise ValueError("duplicate SMS occupied a second record")
    elif outcome == "capacity":
        if pages != 11 or writes != 10:
            raise ValueError("capacity run did not attempt 11 and store 10")
        records = [_record(sim_nvram, number) for number in range(1, 11)]
        if any(record[0] != 0x03 for record in records):
            raise ValueError("capacity run did not retain ten unread records")
        if len(set(records)) != 10:
            raise ValueError("capacity records are not independently identified")
        for reference in range(0x40, 0x4a):
            if f"data=89010202{reference:02x}" not in log:
                raise ValueError(
                    f"missing accepted RP reference {reference:02x}")
        # TS 24.011 RP-ERROR with TS 23.040 cause 0x16: memory capacity
        # exceeded. The network still closes CP and RR normally.
        if "data=890104044a0116" not in log:
            raise ValueError("eleventh message did not report capacity exceeded")
        if len(re.findall(
                r"GSM service downlink kind=17 sapi=3 pd=09 message=04",
                log)) != 11:
            raise ValueError("capacity transactions did not all receive CP-ACK")
        if len(re.findall(
                r"LAPDm service Channel Release acknowledged nr=3",
                log)) != 11:
            raise ValueError("capacity transactions did not all release RR")
        if snapshots is not None:
            hashes = {
                hashlib.sha256(path.read_bytes()).hexdigest()
                for path in snapshots.glob("nokia_dct3_lcdmirror_*.pgm")
            }
            if CAPACITY_UI_SHA256 not in hashes:
                raise ValueError("missing firmware storage-full notification")
    else:
        raise ValueError(f"unsupported outcome {outcome}")


def main() -> int:
    if len(sys.argv) not in (4, 5):
        raise SystemExit(
            "usage: radio_sms_negative_trace_check.py "
            "LOG SIM_NVRAM malformed|duplicate|capacity [SNAPSHOTS]")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes(),
            sys.argv[3],
            pathlib.Path(sys.argv[4]) if len(sys.argv) == 5 else None)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - ordinary SMS negative outcome is {sys.argv[3]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
