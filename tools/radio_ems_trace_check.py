#!/usr/bin/env python3
"""Verify standards-shaped EMS delivery through the ordinary SMS path."""

import pathlib
import re
import sys

try:
    from tools.radio_sms_acceptance_common import frame_hashes
except ModuleNotFoundError:
    from radio_sms_acceptance_common import frame_hashes


SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
FORMATTED_RECORD_PREFIX = bytes.fromhex(
    "01"                    # read
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
PLAIN_RECORD_PREFIX = bytes.fromhex(
    "01"                    # read
    "06912143658709"        # service centre
    "04"                    # SMS-DELIVER
    "0781551532f4"          # sender 5551234
    "00"                    # PID
    "08"                    # UCS-2
    "62704221000000"        # timestamp
    "0a"                    # TP-UDL, octets
    "00680065006c006c006f"  # UCS-2 "hello"
)
MALFORMED_RECORD_PREFIX = bytes.fromhex(
    "01"
    "06912143658709"
    "44"
    "0781551532f4"
    "00"
    "08"
    "62704221000000"
    "10"
    "050a04000510"          # IEDL=4 exceeds the UDHL boundary
    "00680065006c006c006f"
)
RECORD_PREFIXES = {
    "formatted": FORMATTED_RECORD_PREFIX,
    "plain": PLAIN_RECORD_PREFIX,
    "malformed": MALFORMED_RECORD_PREFIX,
}
TEXT_SHA256 = (
    "0dfb0e58ff397aa8cf7891c395deb4fef442d5050606464ee9ab70aa131ae8c4")
ERASE_PROMPT_SHA256 = (
    "c39193e56e48e035d007d0d164c9a3dc51105bb70186202f5c00aaafdd1af1b6")
EMPTY_INBOX_SHA256 = (
    "60077bf89c2f518e6a6c171ef64d7a8b8c50851aea3ea76d291497e8ef849a4d")


def verify(
        text: str,
        sim_nvram: bytes,
        profile: str = "formatted",
        snapshots: pathlib.Path | None = None,
        outcome: str = "read") -> None:
    required = (
        r"PCH IMSI page transmitted channel=60",
        r"sim_device: update fid=6f3c record=1 length=176",
    )
    for pattern in required:
        if not re.search(pattern, text):
            raise ValueError(f"missing EMS transport checkpoint: {pattern}")
    prefix = RECORD_PREFIXES.get(profile)
    if prefix is None:
        raise ValueError(f"unsupported EMS profile {profile!r}")
    stored = sim_nvram[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(prefix)]
    if outcome == "deleted":
        if stored[0] != 0 or stored[1:] != prefix[1:]:
            raise ValueError("EMS deletion did not free the exact stored record")
    elif stored != prefix:
        raise ValueError(f"EF_SMS does not contain the exact read {profile} TPDU")
    hashes = frame_hashes(snapshots) if snapshots is not None else set()
    if snapshots is not None and TEXT_SHA256 not in hashes:
        raise ValueError("firmware did not render the exact UCS-2 text")
    if outcome == "deleted" and (
            ERASE_PROMPT_SHA256 not in hashes or EMPTY_INBOX_SHA256 not in hashes):
        raise ValueError("EMS delete lifecycle did not reach its exact UI states")
    if outcome == "read-state" and "state_roundtrip: result=pass" not in text:
        raise ValueError("missing successful EMS application state replay")


def main() -> int:
    if len(sys.argv) not in (3, 5, 6):
        raise SystemExit(
            "usage: radio_ems_trace_check.py MAME_ERROR_LOG SIM_CARD_NVRAM "
            "[PROFILE SNAPSHOT_DIR [read|read-state|deleted]]")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes(),
            sys.argv[3] if len(sys.argv) >= 5 else "formatted",
            pathlib.Path(sys.argv[4]) if len(sys.argv) >= 5 else None,
            sys.argv[5] if len(sys.argv) == 6 else "read")
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - EMS case reached its exact storage and application boundary")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
