#!/usr/bin/env python3
"""Verify physical mobile-originated SMS rejection by RP-ERROR."""

import hashlib
import pathlib
import re
import sys

try:
    from tools.radio_outgoing_sms_trace_check import CHECKPOINTS
except ModuleNotFoundError:  # Direct execution from tools/.
    from radio_outgoing_sms_trace_check import CHECKPOINTS


MESSAGE_NOT_SENT_HASH = (
    "23bb912a0389c547ac90790f488a6917a4698fbe966f0da90298156197c906dc")


def verify(text: str, frame_directory: pathlib.Path | None = None) -> None:
    # The common physical route ends at the decoded submit and network CP-ACK.
    cursor = 0
    for label, pattern in CHECKPOINTS[:6]:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order rejected-SMS checkpoint: {label}")
        cursor = match.end()

    error = re.search(
        r"GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7",
        text[cursor:])
    if not error:
        raise ValueError("network RP-ERROR was not delivered")
    cursor += error.end()
    final_ack = CHECKPOINTS[7][1].search(text, cursor)
    if not final_ack:
        raise ValueError("handset did not acknowledge RP-ERROR CP-DATA")
    release = CHECKPOINTS[8][1].search(text, final_ack.end())
    if not release:
        raise ValueError("RR channel was not released after RP-ERROR")

    if len(re.findall(r"gsm_sms_submit:", text)) != 1:
        raise ValueError("rejected transaction did not contain exactly one submit")
    if "GSM service downlink kind=18 sapi=3" in text:
        raise ValueError("success RP-ACK appeared in rejected transaction")

    if frame_directory is not None:
        hashes = {
            hashlib.sha256(frame.read_bytes()).hexdigest()
            for frame in frame_directory.glob("nokia_dct3_lcdmirror_*.pgm")
        }
        if MESSAGE_NOT_SENT_HASH not in hashes:
            raise ValueError("firmware Message not sent this time frame absent")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_reject_trace_check.py LOG FRAME_DIR")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - physical SMS-SUBMIT rejected by correlated RP-ERROR")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
