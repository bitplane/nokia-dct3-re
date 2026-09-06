#!/usr/bin/env python3
"""Verify the physical DTMF and hold/retrieve call-control lifecycle."""

import argparse
import hashlib
import pathlib
import re


EVENTS = (
    ("START DTMF", re.compile(r"GSM service uplink .*data=83352c35 ")),
    ("DTMF accepted", re.compile(r"gsm_session: DTMF start digit=35 count=1 ")),
    ("STOP DTMF", re.compile(r"GSM service uplink .*data=8371 ")),
    ("DTMF stopped", re.compile(r"gsm_session: DTMF stop digit=35 count=1 ")),
    ("HOLD", re.compile(r"GSM service uplink .*data=8318 ")),
    ("call held", re.compile(r"gsm_session: call held count=1 ")),
    ("RETRIEVE", re.compile(r"GSM service uplink .*data=835c ")),
    ("call retrieved", re.compile(r"gsm_session: call retrieved count=1 ")),
)

ROUNDTRIP = re.compile(r"state_roundtrip: result=pass")
UNHOLD_FRAME_SHA256 = (
    "ce12ab1791e02096c879f47c80d9a171aaa7dd557eaff23679f276739f52cada"
)


def verify(text: str, frame_dir: pathlib.Path, require_roundtrip: bool = True) -> dict:
    cursor = 0
    for label, pattern in EVENTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order supplementary event: {label}")
        cursor = match.end()

    if require_roundtrip and not ROUNDTRIP.search(text):
        raise ValueError("missing successful held-call save-state roundtrip")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_roundtrip and UNHOLD_FRAME_SHA256 not in hashes:
        raise ValueError("missing exact firmware-rendered active-call Unhold menu")

    return {"events": len(EVENTS), "frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    parser.add_argument("--no-state-roundtrip", action="store_true")
    args = parser.parse_args()
    try:
        result = verify(
            args.log.read_text(errors="replace"), args.frame_dir,
            not args.no_state_roundtrip)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - physical DTMF and hold/retrieve completed in order; "
        f"checked {result['events']} events and the exact Unhold menu frame")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
