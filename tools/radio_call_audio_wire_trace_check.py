#!/usr/bin/env python3
"""Verify the cross-ROM MCU-to-DSP call-audio control wire lifecycle."""

import pathlib
import re
import sys


EVENTS = (
    ("CC Connect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=07 length=2 ")),
    ("answered wire word", re.compile(
        r"dsp_shared_write: off=0a8 old=[0-9a-f]{4} data=860b ")),
    ("Connect Acknowledge", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
        r"03[0-9a-f]{2}09030f")),
    ("active adjacent control", re.compile(
        r"dsp_shared_write: off=0aa old=0000 data=ffff ")),
    ("idle adjacent control", re.compile(
        r"dsp_shared_write: off=0aa old=ffff data=0000 ")),
    ("CC Disconnect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=25 length=5 ")),
    ("network CC Release", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]+09032d")),
    ("CC Release Complete", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=2a length=2 ")),
    ("release channel change", re.compile(
        r"radio_phase=release_channel_change ")),
    ("post-release wire word", re.compile(
        r"dsp_shared_write: off=0a8 old=[0-9a-f]{4} data=840a ")),
    ("idle wire word", re.compile(
        r"dsp_shared_write: off=0a8 old=[0-9a-f]{4} data=8002 ")),
)


def verify(text: str) -> dict[str, int]:
    cursor = 0
    positions = {}
    for label, pattern in EVENTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order cross-ROM audio wire event: {label}")
        positions[label] = match.start()
        cursor = match.end()

    polls = len(re.findall(
        r"TX packet type=1b .*radio_phase=service_uplink_request ",
        text[positions["Connect Acknowledge"]:positions["CC Disconnect"]],
    ))
    if polls < 3:
        raise ValueError("answered interval has fewer than three TCH polls")
    return {"stable_tch_polls": polls, "wire_transitions": 5}


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_call_audio_wire_trace_check.py MAME_ERROR_LOG")
    try:
        result = verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - MCU/DSP audio-control wire followed "
        "8002 -> 860b -> 840a -> 8002 and adjacent control "
        "0000 -> ffff -> 0000 across "
        f"{result['stable_tch_polls']} stable TCH polls")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
