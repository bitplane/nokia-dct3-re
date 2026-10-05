#!/usr/bin/env python3
"""Verify NSM-2 physical mobile-originated signaling, not speech media."""

import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_count, require_ordered
from tools.radio_outgoing_call_trace_check import (
    CM_SERVICE_REQUEST, CM_SERVICE_ACCEPT, SETUP, CALL_PROCEEDING,
    TRAFFIC_ASSIGNMENT, ASSIGNMENT_COMPLETE, ALERTING, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, RELEASE, RELEASE_COMPLETE, RR_RELEASE,
    PCH, decode_called_digits,
)


CHECKPOINTS = (
    ("physical Send", re.compile(r"8850_call_physical: action=send")),
    ("Send decode", re.compile(r"8850_keypad_decoded key=0e\b")),
    ("CM Service Request", CM_SERVICE_REQUEST),
    ("CM Service Accept", CM_SERVICE_ACCEPT),
    ("SETUP", SETUP),
    ("Call Proceeding", CALL_PROCEEDING),
    ("traffic assignment", TRAFFIC_ASSIGNMENT),
    ("own traffic configuration", re.compile(
        r"TX packet type=02 payload=20 .*data=041202000271012fc10000010000000400000000")),
    ("Assignment Complete", ASSIGNMENT_COMPLETE),
    ("Alerting", ALERTING),
    ("Connect", CONNECT),
    ("Connect Acknowledge", CONNECT_ACKNOWLEDGE),
    ("physical End", re.compile(r"8850_call_physical: action=end")),
    ("End decode", re.compile(r"8850_keypad_decoded key=0f\b")),
    ("Disconnect", DISCONNECT),
    ("Release", RELEASE),
    ("Release Complete", RELEASE_COMPLETE),
    ("RR release", RR_RELEASE),
    ("own release configuration", re.compile(
        r"TX packet type=02 payload=20 .*data=041202001117001a600000010000001400000001")),
    ("release confirmation", re.compile(
        r"RX enqueue type=89 payload=8 .*data=0000000000000000")),
    ("return to paging", PCH),
)


def verify(text: str, number: str = "5551234") -> None:
    require_ordered(text, CHECKPOINTS, "NSM-2 physical outgoing signaling")
    for label, pattern in (("SETUP", SETUP), ("assignment", TRAFFIC_ASSIGNMENT),
                           ("Connect Acknowledge", CONNECT_ACKNOWLEDGE),
                           ("Disconnect", DISCONNECT)):
        require_count(text, pattern, 1, f"NSM-2 must emit exactly one {label}")
    setup = SETUP.search(text)
    data = bytes.fromhex(setup.group("data"))
    if len(data) != int(setup.group("length")):
        raise ValueError("SETUP trace payload length mismatch")
    if decode_called_digits(data) != number:
        raise ValueError("SETUP called number differs from physical fixture")


def verify_frames(directory: Path) -> None:
    frames = (
        ("8850_registered_idle.png", "f2dd3c6203be9f67c9fdaf34f8725f7df08f8d17f0a3ab875053c59b0088c2ba"),
        ("8850_after_outgoing_call.png", "59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b"),
    )
    for filename, expected in frames:
        with Image.open(directory / filename) as source:
            frame = source.convert("L")
        if frame.size != (84, 48):
            raise ValueError(f"wrong 8850 frame geometry: {filename}")
        # Operator text only: excludes animated signal/battery indicators.
        digest = hashlib.sha256(frame.crop((15, 0, 69, 16)).tobytes()).hexdigest()
        if digest != expected:
            raise ValueError(f"operator presentation differs: {filename}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--number", default="5551234")
    parser.add_argument("--frames", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.number)
        if args.frames:
            verify_frames(args.frames)
    except (ValueError, OSError) as error:
        print(f"FAIL - {error}", file=sys.stderr)
        return 1
    print("8850 outgoing signaling PASS: physical dial/Send/End and complete CC/RR release")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
