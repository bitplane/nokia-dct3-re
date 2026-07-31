#!/usr/bin/env python3
"""Verify firmware-owned ordinary MT-SMS inbox persistence and UI outcomes."""

import pathlib
import re
import sys

try:
    from tools.radio_sms_acceptance_common import (
        FIRST_SMS_DELIVER_BODY,
        frame_hashes,
        require_single_transport,
        sms_record,
    )
except ModuleNotFoundError:
    from radio_sms_acceptance_common import (
        FIRST_SMS_DELIVER_BODY,
        frame_hashes,
        require_single_transport,
        sms_record,
    )

NOTIFICATION_SHA256 = (
    "beb03cf014becb8ae9576269a405fa51f8063c9596486070a66570d767a06578")
SENDER_SHA256 = frozenset({
    # Notification-originated and ordinary Menu 2-1-1 list paths.
    "413290e06bffe83fc558a614ecba52afbb1331164845021c1476ebd137a03f56",
    "784f281e202711eeb72d5ffece0e1275e77d698e1d40ab12e7e17c3f7b9644cf",
})
TEXT_SHA256 = (
    "0dfb0e58ff397aa8cf7891c395deb4fef442d5050606464ee9ab70aa131ae8c4")
ERASE_PROMPT_SHA256 = (
    "c39193e56e48e035d007d0d164c9a3dc51105bb70186202f5c00aaafdd1af1b6")
EMPTY_INBOX_SHA256 = (
    "60077bf89c2f518e6a6c171ef64d7a8b8c50851aea3ea76d291497e8ef849a4d")

def verify(
        log: str,
        snapshot_dir: pathlib.Path,
        sim_nvram: bytes,
        outcome: str) -> None:
    state_outcome = outcome.endswith("-state")
    base_outcome = outcome.removesuffix("-state")
    if state_outcome and "state_roundtrip: result=pass" not in log:
        raise ValueError("missing successful inbox-boundary state replay")

    hashes = frame_hashes(snapshot_dir)
    record = sms_record(sim_nvram)
    if base_outcome != "cold":
        require_single_transport(log)

    if base_outcome == "delivered":
        if not state_outcome and NOTIFICATION_SHA256 not in hashes:
            raise ValueError("missing firmware '1 message received' state")
        if record[0] != 0x03 or not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
            raise ValueError("EF_SMS record 1 is not the exact unread message")
        state_writes = len(re.findall(
            r"sim_device: update fid=6f3c record=1 length=176", log))
        if state_outcome and state_writes not in (1, 2):
            raise ValueError("state replay produced an invalid storage lifecycle")
        if state_outcome and state_writes == 2 and (
                "state_replay: phase=reference event=begin" not in log or
                "state_replay: phase=restored event=begin" not in log):
            raise ValueError("repeated storage write is not bounded replay")
    elif base_outcome == "dismissed":
        if NOTIFICATION_SHA256 not in hashes:
            raise ValueError("dismiss path did not reach message notification")
        if record[0] != 0x03 or not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
            raise ValueError("notification dismissal did not preserve unread SMS")
        if len(re.findall(
                r"sim_device: update fid=6f3c record=1 length=176",
                log)) != 1:
            raise ValueError("notification dismissal mutated SMS storage")
    elif base_outcome in ("read", "cold"):
        if SENDER_SHA256.isdisjoint(hashes) or TEXT_SHA256 not in hashes:
            raise ValueError(
                "physical inbox path did not expose sender and text")
        if record[0] != 0x01 or not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
            raise ValueError("physical read did not retain exact read EF_SMS")
        updates = len(re.findall(
            r"sim_device: update fid=6f3c record=1 length=176", log))
        if base_outcome == "read" and updates < 2:
            raise ValueError("physical read did not persist the read status")
    elif base_outcome == "deleted":
        if ERASE_PROMPT_SHA256 not in hashes:
            raise ValueError("physical delete did not reach confirmation")
        if EMPTY_INBOX_SHA256 not in hashes:
            raise ValueError("confirmed delete did not reach empty inbox")
        if record[0] != 0x00:
            raise ValueError("confirmed delete did not free EF_SMS record 1")
        if not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
            raise ValueError("firmware deletion unexpectedly rewrote payload")
    elif base_outcome == "cancelled":
        if ERASE_PROMPT_SHA256 not in hashes:
            raise ValueError("cancel path did not reach delete confirmation")
        if record[0] != 0x01 or not record[1:].startswith(FIRST_SMS_DELIVER_BODY):
            raise ValueError("cancelled delete did not preserve read message")
    else:
        raise ValueError(f"unsupported inbox outcome {outcome!r}")


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: radio_sms_inbox_trace_check.py "
            "MAME_ERROR_LOG SNAPSHOT_DIR SIM_NVRAM "
            "delivered|dismissed|read|cold|deleted|cancelled")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]),
            pathlib.Path(sys.argv[3]).read_bytes(),
            sys.argv[4])
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - NSE-8 ordinary MT-SMS inbox outcome is {sys.argv[4]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
