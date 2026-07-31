#!/usr/bin/env python3
"""Check NSE-8 two-message ordering, read isolation, and selective deletion."""

import hashlib
import pathlib
import re
import sys

SMS_NVRAM_OFFSET = 50 * 32 + 11 + 9 + 16
SMS_RECORD_SIZE = 176
FIRST = bytes.fromhex(
    "06912143658709040781551532f400006270422100000005e8329bfd06")
SECOND = bytes.fromhex(
    "06912143658709040781559587f600006270422110000005f7b79c4d06")

HASHES = {
    "notified": {
        "b5682aa281a85a5150bacf2ceba96b1ee6d39fc0fc03001e4c5067a7e41eda9c"},
    "read-first": {
        "7c69f24360ef4479092467be539f244b4caf8642771b39c876f459c8737c776f",
        "0dfb0e58ff397aa8cf7891c395deb4fef442d5050606464ee9ab70aa131ae8c4"},
    "selective-delete": {
        "c39193e56e48e035d007d0d164c9a3dc51105bb70186202f5c00aaafdd1af1b6",
        "1a3b612ddfc0b94d097a6d9d6bc1a55b45e89fd20d0e37816e35b55792acf2f7"},
}


def _record(data: bytes, number: int) -> bytes:
    start = SMS_NVRAM_OFFSET + (number - 1) * SMS_RECORD_SIZE
    return data[start:start + SMS_RECORD_SIZE]


def _frame_hashes(snapshots: pathlib.Path) -> set[str]:
    return {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in snapshots.glob("nokia_dct3_lcdmirror_*.pgm")
    }


def verify(
        log: str,
        snapshots: pathlib.Path,
        sim_nvram: bytes,
        outcome: str) -> None:
    if outcome not in HASHES:
        raise ValueError(f"unsupported outcome {outcome}")
    frame_hashes = _frame_hashes(snapshots)
    missing = HASHES[outcome] - frame_hashes
    if missing:
        raise ValueError(f"missing two-message UI evidence: {sorted(missing)}")
    if len(re.findall(r"PCH IMSI page transmitted channel=60", log)) != 2:
        raise ValueError("two-message UI run did not use two transactions")
    for reference in (0x40, 0x41):
        if f"data=89010202{reference:02x}" not in log:
            raise ValueError(f"missing RP-ACK {reference:02x}")

    first = _record(sim_nvram, 1)
    second = _record(sim_nvram, 2)
    expected = {
        "notified": (3, 3),
        "read-first": (1, 3),
        "selective-delete": (0, 3),
    }[outcome]
    if (first[0], second[0]) != expected:
        raise ValueError(
            f"message statuses are {(first[0], second[0])}, expected {expected}")
    if not first[1:].startswith(FIRST) or not second[1:].startswith(SECOND):
        raise ValueError("selective UI operation altered a message payload")


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: radio_sms_two_message_ui_check.py "
            "LOG SNAPSHOTS SIM_NVRAM notified|read-first|selective-delete")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]),
            pathlib.Path(sys.argv[3]).read_bytes(),
            sys.argv[4])
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - NSE-8 two-message UI outcome is {sys.argv[4]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
