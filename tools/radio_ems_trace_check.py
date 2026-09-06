#!/usr/bin/env python3
"""Verify standards-shaped EMS delivery through the ordinary SMS path."""

import pathlib
import re
import sys


SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
STORED_RECORD_PREFIX = bytes.fromhex(
    "03"                    # unread
    "06912143658709"        # service centre
    "44"                    # SMS-DELIVER, TP-UDHI
    "0781551532f4"          # sender 5551234
    "00"                    # PID
    "08"                    # UCS-2
    "62704221000000"        # timestamp
    "10"                    # TP-UDL, octets
    "050a03000510"          # UDHL + text-formatting IE
    "00680065006c006c006f"  # UCS-2 "hello"
)


def verify(text: str, sim_nvram: bytes) -> None:
    required = (
        r"PCH IMSI page transmitted channel=60",
        r"sim_device: update fid=6f3c record=1 length=176",
    )
    for pattern in required:
        if not re.search(pattern, text):
            raise ValueError(f"missing EMS transport checkpoint: {pattern}")
    stored = sim_nvram[
        SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(STORED_RECORD_PREFIX)]
    if stored != STORED_RECORD_PREFIX:
        raise ValueError("EF_SMS does not contain the exact formatted UCS-2 TPDU")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_ems_trace_check.py MAME_ERROR_LOG SIM_CARD_NVRAM")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(),
               pathlib.Path(sys.argv[2]).read_bytes())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - EMS UDH/UCS-2 payload paged, transported and stored exactly")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
