#!/usr/bin/env python3
"""Verify NHM-5's firmware-owned speech request and independent PCM bus."""

import pathlib
import re
import sys


EVENTS = (
    ("CC Connect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=07 length=2 ")),
    ("speech request", re.compile(
        r"dsp_hle: doorbell pending=[0-9a-f]{4} wire=860b "
        r"speech_control=060b ")),
    ("Connect Acknowledge", re.compile(
        r"RX enqueue type=80 payload=34 .*data=b0[0-9a-f]{18}"
        r"03[0-9a-f]{2}09030f")),
    ("NHM-5 PCM transfer", re.compile(
        r"dsp_hle: speech tick uplink=1 downlink=0 pcm=1 "
        r"pcm_clock=1000000/8000 pcm_shape=125 ")),
    ("physical-End Disconnect", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=25 length=5 ")),
    ("PCM stop", re.compile(
        r"dsp_hle: speech stop control=(?:060b|040a) ")),
    ("speech release", re.compile(
        r"dsp_hle: doorbell pending=[0-9a-f]{4} wire=840a "
        r"speech_control=040a ")),
)


def verify(text: str) -> None:
    cursor = 0
    for label, pattern in EVENTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order NHM-5 speech-control event: {label}")
        cursor = match.end()

def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_3310_speech_control_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print(
        "OK - NHM-5 requested and released speech across its independent "
        "1 MHz/125-clock PCM bus")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
