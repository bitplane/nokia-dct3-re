#!/usr/bin/env python3
"""Verify save/load begins after remote Disconnect and replays release exactly."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_outgoing_call_outcome_trace_check import (
    HANDSET_RELEASE,
    NETWORK_DISCONNECT,
)
from tools.radio_outgoing_call_trace_check import PCH, RR_RELEASE


ROUNDTRIP = re.compile(
    r"state_roundtrip: result=pass .*?requested_at=(?P<time>[0-9.]+)"
)
TIMESTAMP = re.compile(r"t=(?P<time>[0-9.]+)")
MARKER = re.compile(
    r"state_replay: phase=(reference|restored) event=(begin|end)"
)
REPLAY_TOKENS = (
    "GSM service ",
    "LAPDm service ",
    "speech stop ",
    "radio_l1: ",
)


def timestamp_after(text: str, match: re.Match) -> float:
    found = TIMESTAMP.search(text, match.end(), text.find("\n", match.end()))
    if not found:
        raise ValueError("release record omitted emulated timestamp")
    return float(found.group("time"))


def replay_records(lines, begin, end):
    return [
        line[line.index(token):]
        for line in lines[begin + 1:end]
        for token in REPLAY_TOKENS
        if token in line
    ]


def check(text: str) -> None:
    roundtrip = ROUNDTRIP.search(text)
    disconnect = NETWORK_DISCONNECT.search(text)
    if not roundtrip or not disconnect:
        raise ValueError("missing release save/load or network Disconnect")
    requested = float(roundtrip.group("time"))
    disconnected = timestamp_after(text, disconnect)
    if not disconnected < requested < disconnected + 0.05:
        raise ValueError("save/load did not begin inside the release exchange")

    lines = text.splitlines()
    markers = [
        (match.group(1), match.group(2), index)
        for index, line in enumerate(lines)
        if (match := MARKER.search(line))
    ]
    if [(phase, event) for phase, event, _ in markers] != [
        ("reference", "begin"),
        ("reference", "end"),
        ("restored", "begin"),
        ("restored", "end"),
    ]:
        raise ValueError("missing deterministic release replay markers")
    reference = replay_records(lines, markers[0][2], markers[1][2])
    restored = replay_records(lines, markers[2][2], markers[3][2])
    if not reference or reference != restored:
        raise ValueError("restored CC/RR release trace diverged")

    releases = list(HANDSET_RELEASE.finditer(text))
    rr_releases = list(RR_RELEASE.finditer(text))
    if len(releases) < 2 or len(rr_releases) < 2:
        raise ValueError("release tail was not executed in both timelines")
    if not PCH.search(text, roundtrip.end()):
        raise ValueError("restored release did not return to PCH")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - save/load deterministically replayed the active CC/RR release")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
