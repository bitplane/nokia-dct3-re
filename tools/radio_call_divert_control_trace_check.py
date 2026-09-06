#!/usr/bin/env python3
"""Verify organic call-forward registration or deactivation."""

import argparse
import hashlib
import pathlib
import re


CASES = {
    "register": {
        "request": re.compile(
            r"GSM service uplink .*pd=0b message=3b length=27 "
            r"data=1b7b1c14a11202010102010a300a040121840581551532f47f0100 "),
        "decoded": re.compile(
            r"gsm_ss: request=register transaction=1b invoke=1 service=21 "
            r"number_length=5 active=1 "),
        "frame": "b2828c0fd7a73608f6c35b71a1092655153e0620bd56ca12dd5ee9987d826b6d",
    },
    "deactivate": {
        "request": re.compile(
            r"GSM service uplink .*pd=0b message=3b length=20 "
            r"data=1b7b1c0da10b02010102010d30030401217f0100 "),
        "decoded": re.compile(
            r"gsm_ss: request=deactivate transaction=1b invoke=1 service=21 "
            r"active=0 "),
        "frame": "df58fd177586c6b223ca870ff90677821a2dd23e015940f834797a45dad747e5",
    },
}
RESPONSE = re.compile(
    r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=14 ")
RR_RELEASE = re.compile(r"radio_phase=release_channel_change")


def verify(operation: str, text: str, frame_dir: pathlib.Path,
           require_frame: bool = True) -> dict:
    case = CASES[operation]
    request = case["request"].search(text)
    if not request:
        raise ValueError(f"missing exact organic {operation} request")
    decoded = case["decoded"].search(text, request.end())
    if not decoded:
        raise ValueError(f"missing decoded {operation} operation")
    response = RESPONSE.search(text, decoded.end())
    if not response:
        raise ValueError(f"missing correlated {operation} result")
    if not RR_RELEASE.search(text, response.end()):
        raise ValueError(f"missing RR release after {operation}")
    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_frame and case["frame"] not in hashes:
        raise ValueError(f"missing exact firmware-rendered {operation} frame")
    return {"frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("operation", choices=CASES)
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.operation, args.log.read_text(errors="replace"),
               args.frame_dir)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - organic call-forward {args.operation} completed and released RR")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
