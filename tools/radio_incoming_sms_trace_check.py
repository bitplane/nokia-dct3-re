#!/usr/bin/env python3
"""Verify one organic network-originated ordinary text SMS delivery."""

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
    ("SAPI 3 SABM", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"0f3f01")),
    ("SAPI 3 UA", re.compile(
        r"TX packet type=1b .*data=00800f7301")),
    ("SMS CP-DATA segment 1", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"0f00530901210140069121436587090016040781551532")),
    ("segment 1 stop-and-wait acknowledgement", re.compile(
        r"TX packet type=1b .*data=00800f2101")),
    ("SMS CP-DATA segment 2", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
        r"0f0241f400006270422100000005e8329bfd06")),
    ("segment 2 stop-and-wait acknowledgement", re.compile(
        r"TX packet type=1b .*data=00800f4101")),
    ("EF_SMS record update", re.compile(
        r"sim_device: update fid=6f3c record=1 length=176")),
)

SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
STORED_RECORD_PREFIX = bytes.fromhex(
    "03"
    "06912143658709"
    "040781551532f4"
    "0000"
    "62704221000000"
    "05e8329bfd06"
)


def verify(text: str, sim_nvram: bytes) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order incoming-SMS checkpoint: {label}")
        cursor = match.end()

    pages = re.findall(r"PCH IMSI page transmitted channel=60", text)
    if len(pages) != 1:
        raise ValueError(f"expected exactly one IMSI page, observed {len(pages)}")

    sabms = re.findall(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}0f3f01",
        text)
    if len(sabms) != 1:
        raise ValueError(
            f"expected exactly one SAPI 3 SABM, observed {len(sabms)}")

    stored = sim_nvram[
        SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(STORED_RECORD_PREFIX)]
    if stored != STORED_RECORD_PREFIX:
        raise ValueError(
            "EF_SMS record 1 does not contain the expected unread "
            'SMS-DELIVER for "hello"')


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_incoming_sms_trace_check.py "
            "MAME_ERROR_LOG SIM_CARD_NVRAM")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        'OK - page, SC=0 DSP cipher control/complete, SAPI 3 segmented '
        'SMS-DELIVER and persistent unread "hello" record completed organically')
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
