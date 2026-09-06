#!/usr/bin/env python3
"""Verify organic unconditional-call-forwarding interrogation."""

import argparse
import hashlib
import pathlib
import re


REQUEST = re.compile(
    r"GSM service uplink .*pd=0b message=3b length=20 "
    r"data=1b7b1c0da10b02010102010e30030401217f0100 ")
DECODED = re.compile(
    r"gsm_ss: request=interrogate transaction=1b invoke=1 "
    r"service=21 active=0 ")
RESPONSE = re.compile(
    r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=17 ")
RR_RELEASE = re.compile(r"radio_phase=release_channel_change")
NOT_ACTIVE_FRAME = "e1f8fa4d2791e6a1c0c7cf7afe78d4e64be7d9c7c689a4539c893a420a9df224"


def verify(text: str, frame_dir: pathlib.Path, require_frame: bool = True) -> dict:
    request = REQUEST.search(text)
    if not request:
        raise ValueError("missing exact InterrogateSS request for service 0x21")
    decoded = DECODED.search(text, request.end())
    if not decoded:
        raise ValueError("missing correlated supplementary-service decode")
    response = RESPONSE.search(text, decoded.end())
    if not response:
        raise ValueError("missing supplementary RELEASE COMPLETE result")
    if not RR_RELEASE.search(text, response.end()):
        raise ValueError("missing RR release after supplementary result")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_frame and NOT_ACTIVE_FRAME not in hashes:
        raise ValueError("missing exact firmware-rendered Service not active frame")
    return {"frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.frame_dir)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - organic *#21# interrogation returned Service not active and released RR")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
