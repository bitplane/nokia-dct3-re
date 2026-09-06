#!/usr/bin/env python3
"""Verify NSE-8 call waiting, swap, and independent CC release."""

import argparse
import hashlib
import pathlib
import re


EVENTS = (
    ("first CONNECT", re.compile(r"GSM service uplink .*data=8347 ")),
    ("second SETUP", re.compile(
        r"waiting call SETUP transaction=13 malformed=0 duplicate=0 ")),
    ("second CALL CONFIRMED", re.compile(r"GSM service uplink .*data=9308")),
    ("second ALERTING", re.compile(r"GSM service uplink .*data=9341 ")),
    ("hold first", re.compile(r"call held count=1 transaction=83 leg=0 ")),
    ("second CONNECT", re.compile(r"GSM service uplink .*data=9347 ")),
    ("hold second", re.compile(r"call held count=2 transaction=93 leg=1 ")),
    ("retrieve first", re.compile(
        r"call retrieved count=1 transaction=83 leg=0 ")),
    ("release first", re.compile(r"GSM service uplink .*data=832502e090 ")),
    ("release-complete first", re.compile(r"GSM service uplink .*data=836a ")),
    ("retrieve second", re.compile(
        r"call retrieved count=2 transaction=93 leg=1 ")),
    ("release second", re.compile(r"GSM service uplink .*data=936502e090 ")),
    ("release-complete second", re.compile(r"GSM service uplink .*data=932a ")),
)

ROUNDTRIP = re.compile(r"state_roundtrip: result=pass")
RR_RELEASE = re.compile(r"radio_phase=release_channel_change")
REQUIRED_FRAMES = {
    "call-waiting answer menu":
        "749e057ce32db289431640f1e89bbdca2640da1110d103b7e18a3b27769c21cf",
    "two-call swap menu":
        "9b31909501d9ee4e50d66514d2add8031eba0baca5fdf31c196fdf0cb225e0c1",
}


def verify(text: str, frame_dir: pathlib.Path, require_frames: bool = True) -> dict:
    cursor = 0
    positions = {}
    for label, pattern in EVENTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order two-call event: {label}")
        positions[label] = match.start()
        cursor = match.end()

    if RR_RELEASE.search(text[positions["release first"]:positions["retrieve second"]]):
        raise ValueError("RR channel released while the second call remained live")
    if not RR_RELEASE.search(text[positions["release-complete second"]:]):
        raise ValueError("missing RR teardown after the final call leg")
    if not ROUNDTRIP.search(text):
        raise ValueError("missing successful two-call save-state roundtrip")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_frames:
        for label, expected in REQUIRED_FRAMES.items():
            if expected not in hashes:
                raise ValueError(f"missing exact firmware-rendered {label}")
    return {"events": len(EVENTS), "frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = verify(args.log.read_text(errors="replace"), args.frame_dir)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - call waiting, transaction-local hold/swap/release, save-state, "
        f"and final RR teardown reproduced across {result['events']} events")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
