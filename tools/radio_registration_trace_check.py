#!/usr/bin/env python3
"""Verify one organic GSM registration and return-to-idle lifecycle."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("random access", re.compile(r"TX packet type=0c .*data=0000[0-9a-f]{2}")),
    ("Location Updating Request", re.compile(
        r"TX packet type=1b .*data=0080013f[0-9a-f]*490508")),
    ("contention-resolution UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*"
        r"01734905087000f000fffe330809101010325476982b2b2b")),
    ("Location Updating Accept", re.compile(
        r"radio_mm_parse: phase=return .*result=00000048 ")),
    ("operator presentation", re.compile(
        r"operator-resource data=.*00.*f1.*10.*01")),
    ("EF_LOCI location update", re.compile(
        r"sim_device: update-binary fid=6f7e offset=4 length=5")),
    ("EF_LOCI status update", re.compile(
        r"sim_device: update-binary fid=6f7e offset=10 length=1")),
)

POST_ACCEPT_CHECKPOINTS = (
    ("RR channel deconfiguration", re.compile(
        r"TX packet type=02 .*radio_phase=release_channel_change "
        r"data=041202000000001a600000010000000f00000000")),
    ("idle-channel confirmation", re.compile(r"radio peer RX type=89 .*")),
)

def verify(text: str) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order registration checkpoint: {label}")
        cursor = match.end()

    for label, pattern in POST_ACCEPT_CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order registration checkpoint: {label}")
        cursor = match.end()

    requests = re.findall(r"TX packet type=1b .*data=0080013f[0-9a-f]*490508", text)
    if len(requests) != 1:
        raise ValueError(f"expected one Location Updating Request, observed {len(requests)}")

    release = text.find("radio_phase=release_channel_change")
    steady_bcch = len(re.findall(
        r"RX enqueue type=80 payload=34 .*data=50", text[release:]))
    if steady_bcch < 4:
        raise ValueError(
            f"serving BCCH did not remain active after release: {steady_bcch} blocks")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: radio_registration_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - Location Updating accepted, channel released, and operator resource published")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
