#!/usr/bin/env python3
"""Classify firmware Smart Message envelope handling above closed transport."""

import pathlib
import re
import sys


SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
FREE_SMS_RECORD_PREFIX = bytes([0x00]) + bytes([0xff]) * 35
DOWNLINK = re.compile(
    r"GSM service downlink kind=16 sapi=3 pd=09 message=01 .*t=([0-9.]+)")
PUP = re.compile(r"buzzer: enabled=1 .*t=([0-9.]+)")

EXPECTED_NOTIFICATIONS = {
    "valid": (False, True),
    "missing": (False,),
    "bad-reference": (False, False),
    "bad-total": (False, False),
    "wrong-port": (False, False),
    "reversed": (False, True),
    "duplicate": (False, True),
    "truncated-udh": (True, True),
    "stale-then-valid": (False, False, True),
}


def verify(log: str, sim_nvram: bytes, profile: str) -> None:
    expected = EXPECTED_NOTIFICATIONS.get(profile)
    if expected is None:
        raise ValueError(f"unsupported envelope profile {profile!r}")
    deliveries = [float(value) for value in DOWNLINK.findall(log)]
    if len(deliveries) != len(expected):
        raise ValueError(
            f"{profile}: expected {len(expected)} CP-DATA deliveries, "
            f"observed {len(deliveries)}")
    notes = [float(value) for value in PUP.findall(log)]
    observed = tuple(
        any(delivery < note < delivery + 3.0 for note in notes)
        for delivery in deliveries)
    if observed != expected:
        raise ValueError(
            f"{profile}: expected application notifications {expected}, "
            f"observed {observed}")
    if "sim_device: update fid=6f3c" in log:
        raise ValueError(f"{profile}: envelope mutated EF_SMS")
    stored = sim_nvram[
        SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(FREE_SMS_RECORD_PREFIX)]
    if stored != FREE_SMS_RECORD_PREFIX:
        raise ValueError(f"{profile}: EF_SMS record 1 changed")


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: radio_smart_message_envelope_trace_check.py "
            "MAME_ERROR_LOG SIM_CARD_NVRAM PROFILE")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes(),
            sys.argv[3])
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - firmware Smart Message envelope behavior matches {sys.argv[3]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
