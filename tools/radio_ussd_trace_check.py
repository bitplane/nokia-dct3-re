#!/usr/bin/env python3
"""Verify an organic call-independent USSD request and response."""

import argparse
import hashlib
import pathlib
import re


REQUEST = re.compile(
    r"GSM service uplink .*pd=0b message=3b length=27 "
    r"data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 ")
DECODED = re.compile(
    r"gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 ")
RESPONSE = re.compile(
    r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37 ")
RR_RELEASE = re.compile(r"radio_phase=release_channel_change")
RESULT_FRAME = "cf2e4a3461da27d5c49c2077810f57cc2caf6e295b089021c25157000b6324b7"


def verify(text: str, frame_dir: pathlib.Path, require_frame: bool = True) -> dict:
    request = REQUEST.search(text)
    if not request:
        raise ValueError("missing exact processUnstructuredSS-Request for *123#")
    decoded = DECODED.search(text, request.end())
    if not decoded:
        raise ValueError("missing correlated USSD DCS/payload decode")
    response = RESPONSE.search(text, decoded.end())
    if not response:
        raise ValueError("missing USSD ReturnResult RELEASE COMPLETE")
    if not RR_RELEASE.search(text, response.end()):
        raise ValueError("missing RR release after USSD result")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_frame and RESULT_FRAME not in hashes:
        raise ValueError("missing exact firmware-rendered Nokia test network frame")
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
    print("OK - organic *123# returned and rendered a correlated USSD result")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
