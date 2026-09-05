#!/usr/bin/env python3
"""Verify save-state continuation during an outgoing-SMS CP wait."""

import pathlib
import re
import sys

try:
    from tools.radio_outgoing_sms_timeout_trace_check import verify as verify_timeout
except ModuleNotFoundError:  # Direct execution from tools/.
    from radio_outgoing_sms_timeout_trace_check import verify as verify_timeout


def verify(text: str, frame_directory: pathlib.Path | None = None) -> None:
    verify_timeout(text, frame_directory)
    submit = re.search(r"gsm_sms_submit: .* outcome=2 t=([0-9.]+)", text)
    restored = re.search(
        r"state_roundtrip: result=(\w+) .*?requested_at=([0-9.]+) "
        r"t=([0-9.]+)", text)
    retry = re.search(
        r"TX pending type=1b payload=25 data=00800d04.*? t=([0-9.]+)",
        text)
    if not restored or restored.group(1) != "pass":
        raise ValueError("outgoing-SMS CP-wait state round trip did not pass")
    if not submit or not retry:
        raise ValueError("submit or retry timestamp absent")
    submit_time = float(submit.group(1))
    save_time = float(restored.group(2))
    retry_time = float(retry.group(1))
    if not submit_time < save_time < retry_time:
        raise ValueError(
            "state round trip did not occur between SMS-SUBMIT and CP retry")


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: radio_outgoing_sms_timeout_state_trace_check.py "
            "LOG FRAME_DIR")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - outgoing SMS CP timeout resumed across save-state restoration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
