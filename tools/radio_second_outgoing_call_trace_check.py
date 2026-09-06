#!/usr/bin/env python3
"""Verify NSE-8's second mobile-originated call on an existing TCH."""

import argparse
import hashlib
import pathlib
import re


EVENTS = (
    ("first CONNECT", re.compile(r"GSM service uplink .*data=8347 ")),
    ("hold first", re.compile(
        r"call held count=1 transaction=83 leg=0 ")),
    ("nested CM service", re.compile(
        r"second outgoing CM service request transaction=05 live_legs=1 ")),
    ("second SETUP", re.compile(
        r"second outgoing SETUP transaction=13 leg=1 digits=8 ")),
    ("second CONNECT ACK", re.compile(r"GSM service uplink .*data=134f ")),
)

UPLINK = re.compile(r"GSM service uplink .*data=([0-9a-f]+) ")

# These are all six scroll positions in the two-call Options menu.  Together
# they make the absence of a Conference command a bounded NSE-8 UI result.
TWO_CALL_MENU_FRAMES = {
    "end this call":
        "9b31909501d9ee4e50d66514d2add8031eba0baca5fdf31c196fdf0cb225e0c1",
    "swap":
        "68cb944aa9c61ca43009c6ef51afdb54897c8f36f9479e6495a1eaeb3634adac",
    "end all calls":
        "cf32ebe58cc34971ae1cdc534015e208ee5138045f806ed49022c0c06fd813d6",
    "send DTMF":
        "c80b8f2d4a7c67ecdf3db2b3d123f5866d2b8e91f8692fd11b720d57081445d8",
    "send":
        "6cceb95feb87e213dcf35e638d39a3e033115815550dee5a8d43aa7fcfe24318",
    "phone book":
        "046a9182f51ad1618f246c28deae22f1b5585d5e9bded24b7d699fe197a840e0",
}


def _has_call_related_facility(text: str) -> bool:
    for match in UPLINK.finditer(text):
        data = bytes.fromhex(match.group(1))
        if len(data) >= 2 and data[0] & 0x0f == 0x03 and data[1] & 0x3f == 0x3a:
            return True
    return False


def verify(text: str, frame_dir: pathlib.Path, require_frames: bool = True) -> dict:
    cursor = 0
    for label, pattern in EVENTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order second-call event: {label}")
        cursor = match.end()

    if _has_call_related_facility(text):
        raise ValueError("unexpected call-related FACILITY request from NSE-8")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_frames:
        for label, expected in TWO_CALL_MENU_FRAMES.items():
            if expected not in hashes:
                raise ValueError(f"missing exact two-call menu position: {label}")

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
        "OK - second mobile-originated call used the existing TCH; "
        f"checked {result['events']} events and all six NSE-8 menu positions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
