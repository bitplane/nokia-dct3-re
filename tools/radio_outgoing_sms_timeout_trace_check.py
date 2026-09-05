#!/usr/bin/env python3
"""Verify physical MO-SMS CP timeout and bounded retry behavior."""

import hashlib
import pathlib
import re
import sys

try:
    from tools.radio_outgoing_sms_trace_check import CHECKPOINTS
except ModuleNotFoundError:  # Direct execution from tools/.
    from radio_outgoing_sms_trace_check import CHECKPOINTS


MESSAGE_SENDING_FAILED_HASH = (
    "b1d4d4e832e64df9cf4cf28eeae3a30cc3f24af3f5d4f60d1e22243e092dfc1c")
SEGMENT = "53290119000100069121436587090e110007815515"


def verify(text: str, frame_directory: pathlib.Path | None = None) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS[:5]:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order timed-out-SMS checkpoint: {label}")
        cursor = match.end()

    submit = re.search(
        r"gsm_sms_submit: .* outcome=2 status_report=[01] t=([0-9.]+)",
        text)
    if not submit:
        raise ValueError("CP-silence submit outcome was not selected")
    submit_time = float(submit.group(1))

    retries = [
        (int(match.group(1), 16), float(match.group(2)))
        for match in re.finditer(
            rf"TX pending type=1b payload=25 data=00800d([0-9a-f]{{2}})"
            rf"{SEGMENT} t=([0-9.]+)", text)
    ]
    if [control for control, _ in retries] != [0x00, 0x04, 0x14]:
        raise ValueError(
            f"expected initial/CP-retry/T200-poll controls 00,04,14; got "
            f"{[f'{control:02x}' for control, _ in retries]}")
    cp_retry_delay = retries[1][1] - submit_time
    poll_delay = retries[2][1] - retries[1][1]
    if not 20.0 <= cp_retry_delay <= 30.0:
        raise ValueError(f"CP retry delay {cp_retry_delay:.3f}s outside evidence")
    if not 0.3 <= poll_delay <= 1.2:
        raise ValueError(f"T200 poll delay {poll_delay:.3f}s outside evidence")
    if "data=801200" not in text or "00000d7101" not in text:
        raise ValueError("RR response did not carry N(R)=3 and F=1")
    if "GSM service downlink kind=17 sapi=3" in text:
        raise ValueError("network CP-ACK appeared in CP-silence transaction")

    if frame_directory is not None:
        hashes = {
            hashlib.sha256(frame.read_bytes()).hexdigest()
            for frame in frame_directory.glob("nokia_dct3_lcdmirror_*.pgm")
        }
        if MESSAGE_SENDING_FAILED_HASH not in hashes:
            raise ValueError("firmware Message sending failed frame absent")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_timeout_trace_check.py LOG FRAME_DIR")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - physical SMS-SUBMIT CP timeout and bounded retry completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
