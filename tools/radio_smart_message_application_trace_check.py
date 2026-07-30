#!/usr/bin/env python3
"""Check the firmware-owned Nokia Smart Message application frontier."""

import hashlib
import pathlib
import re
import sys


PLAYING_ACCEPTED_SHA256 = (
    "29a83bf05ca55ebffb02eb740387c04c284701c8e38030443e13e71ffbb31a51")
PLAYING_REJECTED_SHA256 = (
    "9c8fa94a27622af54e9716bd3e1bb7740e63652723ab9f7b197492b81360683d")
PLAYING_3410_SHA256 = (
    "901e5520b49c62a16dfdef03c67cdb72b6a4109dd3f8d9735f00fa3f309c402d")
PLAYING_3310_SHA256 = (
    "21433f4cec82fd5a12a513778fe32826f82f4f95b3258a5e22cdc8c7bb359548")
PLAYING_NO_TERMINATOR_SHA256 = (
    "fd76b151654dd0c496408189516d99952bddd67a3be8a4cdd90604a22df37636")

DOWNLINK = re.compile(
    r"GSM service downlink kind=16 sapi=3 pd=09 message=01 .*t=([0-9.]+)")
PUP_NOTE = re.compile(
    r"buzzer: enabled=1 divider=(\d+) frequency=(\d+) volume=(\d+) "
    r"t=([0-9.]+)")


def verify(log: str, frame: bytes, outcome: str) -> None:
    deliveries = [float(value) for value in DOWNLINK.findall(log)]
    if len(deliveries) != 2:
        raise ValueError(
            f"expected two complete Smart Message CP-DATA deliveries, "
            f"observed {len(deliveries)}")
    if not re.search(r"LAPDm service Channel Release acknowledged nr=", log):
        raise ValueError("missing firmware-owned release after Smart Message")

    arrival = [
        (int(divider), int(frequency), int(volume), float(time))
        for divider, frequency, volume, time in PUP_NOTE.findall(log)
        if deliveries[1] <= float(time) < deliveries[1] + 4.0
    ]
    if not arrival:
        raise ValueError("missing completed-message application notification")

    playback = [
        (int(divider), int(frequency), int(volume), float(time))
        for divider, frequency, volume, time in PUP_NOTE.findall(log)
        if float(time) >= deliveries[1] + 5.0
    ]
    digest = hashlib.sha256(frame).hexdigest()
    state_outcome = outcome.endswith("-state")
    base_outcome = outcome.removesuffix("-state")
    if state_outcome and "state_roundtrip: result=pass" not in log:
        raise ValueError("missing successful application-boundary state replay")
    if base_outcome in (
            "accepted", "accepted-3310", "accepted-3410",
            "accepted-duplicate", "accepted-no-terminator"):
        expected_digest = {
            "accepted": PLAYING_ACCEPTED_SHA256,
            "accepted-3310": PLAYING_3310_SHA256,
            "accepted-3410": PLAYING_3410_SHA256,
            "accepted-duplicate": PLAYING_NO_TERMINATOR_SHA256,
            "accepted-no-terminator": PLAYING_NO_TERMINATOR_SHA256,
        }[base_outcome]
        if digest != expected_digest:
            raise ValueError(
                "missing stable firmware ringtone playback screen")
        frequencies = {frequency for _, frequency, _, _ in playback
                       if frequency < 10000}
        if len(frequencies) < 5:
            raise ValueError(
                "RTPL Play did not produce the expected note-varying PUP "
                "sequence")
    elif base_outcome == "rejected":
        if digest != PLAYING_REJECTED_SHA256:
            raise ValueError(
                "missing stable untitled Playing tone/quit rejection screen")
        if playback:
            raise ValueError(
                "commandless RTPL unexpectedly produced playback notes")
    else:
        raise ValueError(f"unsupported expected outcome {outcome!r}")


def main() -> int:
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: radio_smart_message_application_trace_check.py "
            "MAME_ERROR_LOG FINAL_LCD_PGM accepted|rejected")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes(),
            sys.argv[3])
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        f"OK - firmware-owned Smart Message application outcome is "
        f"{sys.argv[3]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
