#!/usr/bin/env python3
"""Verify organic network-initiated USSD request and notification outcomes."""

import argparse
import hashlib
import pathlib
import re


PAGE = re.compile(r"PCH IMSI page transmitted channel=60 fn=")
PAGING_RESPONSE = re.compile(
    r"GSM service establish sapi=0 pd=06 message=27 length=")
REQUEST = re.compile(
    r"gsm_ss: network_initiated operation=(?P<operation>request|notify) "
    r"transaction=0b invoke=1 dcs=0f ")
REGISTER = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=0b message=3b "
    r"length=(?P<length>\d+) ")
REQUEST_REJECTION = re.compile(
    r"gsm_ss: network_initiated handset_response message=2a length=6 "
    r"data=8b6a0802e0e0 ")
NOTIFICATION_RESULT = re.compile(
    r"gsm_ss: network_initiated handset_response message=3a length=8 "
    r"data=8b7a05a203020101 ")
NETWORK_RELEASE = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=0b message=2a length=2 ")
RR_RELEASE = re.compile(r"LAPDm service Channel Release acknowledged")
NOTIFICATION_FRAME = (
    "e3ae95ae65fd04450bd53deb5516477cd586e6bd252b5affa527d4f8b5e2d9d3")


def verify(text: str, frame_dir: pathlib.Path, outcome: str,
           require_frame: bool = True) -> dict:
    cursor = 0
    for label, pattern in (("IMSI page", PAGE), ("Paging Response", PAGING_RESPONSE)):
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order {label}")
        cursor = match.end()

    request = REQUEST.search(text, cursor)
    if not request or request.group("operation") != outcome:
        raise ValueError(f"missing network-originated USSD {outcome}")
    register = REGISTER.search(text, request.end())
    if not register:
        raise ValueError("missing network-originated REGISTER")

    if outcome == "request":
        if register.group("length") != "32":
            raise ValueError("wrong network USSD request length")
        response = REQUEST_REJECTION.search(text, register.end())
        if not response:
            raise ValueError("missing exact handset request rejection")
    else:
        if register.group("length") != "35":
            raise ValueError("wrong network USSD notification length")
        response = NOTIFICATION_RESULT.search(text, register.end())
        if not response:
            raise ValueError("missing exact handset notification result")
        release = NETWORK_RELEASE.search(text, response.end())
        if not release:
            raise ValueError("missing network RELEASE COMPLETE")
        response = release

    if not RR_RELEASE.search(text, response.end()):
        raise ValueError("missing RR release after network USSD")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if outcome == "notify" and require_frame and NOTIFICATION_FRAME not in hashes:
        raise ValueError("missing exact firmware-rendered USSD notification frame")
    return {"frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    parser.add_argument("--outcome", choices=("request", "notify"), required=True)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.frame_dir, args.outcome)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - network-initiated USSD {args.outcome} contract completed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
