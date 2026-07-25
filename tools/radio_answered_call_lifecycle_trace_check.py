#!/usr/bin/env python3
"""Verify organic physical Answer-to-End call and DSP-control lifecycle."""

import pathlib
import re
import sys


EVENTS = (
    (
        "CC Connect",
        re.compile(r"GSM service uplink sapi=0 pd=03 message=07 length=2 "),
    ),
    (
        "connected-state first producer",
        re.compile(
            r"dsp_audio_shadow_write: address=0011206c old=0002 data=0203 "
            r"pc=0028d9a8 task=05 "),
    ),
    (
        "connected-state second producer",
        re.compile(
            r"dsp_audio_shadow_write: address=0011206c old=0203 data=060b "
            r"pc=0028dd1c task=05 "),
    ),
    (
        "connected-state publication",
        re.compile(
            r"dsp_shared_control: command=08 value=060b commit=1 .*task=05 "),
    ),
    (
        "Connect Acknowledge",
        re.compile(
            r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
            r"03[0-9a-f]{2}09030f"),
    ),
    (
        "physical-End CC Disconnect",
        re.compile(r"GSM service uplink sapi=0 pd=03 message=25 length=5 "),
    ),
    (
        "network CC Release",
        re.compile(
            r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]+09032d"),
    ),
    (
        "CC Release Complete",
        re.compile(r"GSM service uplink sapi=0 pd=03 message=2a length=2 "),
    ),
    (
        "release channel change",
        re.compile(r"radio_phase=release_channel_change "),
    ),
    (
        "post-release first producer",
        re.compile(
            r"dsp_audio_shadow_write: address=0011206c old=060b data=060a "
            r"pc=0028d97e task=09 "),
    ),
    (
        "post-release second producer",
        re.compile(
            r"dsp_audio_shadow_write: address=0011206c old=060a data=040a "
            r"pc=0028d986 task=09 "),
    ),
    (
        "post-release publication",
        re.compile(
            r"dsp_shared_control: command=08 value=040a commit=1 .*task=09 "),
    ),
    (
        "idle-state producer",
        re.compile(
            r"dsp_audio_shadow_write: address=0011206c old=040a data=0002 "
            r"pc=0028dcf6 task=09 "),
    ),
    (
        "idle-state publication",
        re.compile(
            r"dsp_shared_control: command=08 value=0002 commit=1 .*task=09 "),
    ),
)


def verify(text: str) -> dict[str, int]:
    cursor = 0
    positions = {}
    for label, pattern in EVENTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order call lifecycle: {label}")
        positions[label] = match.start()
        cursor = match.end()

    stable_polls = len(re.findall(
        r"TX packet type=1b .*radio_phase=service_uplink_request ",
        text[positions["Connect Acknowledge"]:
             positions["physical-End CC Disconnect"]],
    ))
    if stable_polls < 3:
        raise ValueError(
            "answered interval did not retain at least three organic TCH polls")

    return {
        "stable_tch_polls": stable_polls,
        "control_states": 3,
    }


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_answered_call_lifecycle_trace_check.py "
            "MAME_ERROR_LOG")
    try:
        result = verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - physical Answer and End completed CC teardown and the observed "
        "DSP control lifecycle 0002 -> 060b -> 040a -> 0002 across "
        f"{result['stable_tch_polls']} stable TCH polls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
