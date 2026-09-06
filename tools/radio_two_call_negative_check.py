#!/usr/bin/env python3
"""Check bounded duplicate and malformed call-waiting compositions."""

import argparse
import hashlib
import pathlib
import re


WAITING_FRAME = "b95beac731df4b62b6ff1bed7cd596cae2193d6899c61d114fcf8cf7bb22f4ef"


def verify(text: str, frame_dir: pathlib.Path, profile: str) -> dict:
    connect = "GSM service uplink sapi=0 pd=03 message=07 length=2 data=8347 "
    connect_at = text.find(connect)
    if connect_at < 0:
        raise ValueError("first call did not connect organically")
    if "radio_phase=release_channel_change" in text[connect_at:]:
        raise ValueError("negative call-waiting input released the live RR channel")

    confirmations = len(re.findall(r"GSM service uplink .*data=9308", text))
    alerting = len(re.findall(r"GSM service uplink .*data=9341 ", text))
    if profile == "duplicate":
        if text.count("malformed=0 duplicate=0") != 1 or \
                text.count("malformed=0 duplicate=1") != 1:
            raise ValueError("duplicate profile did not transmit exactly two SETUPs")
        if confirmations != 1 or alerting != 1:
            raise ValueError("duplicate SETUP created another call-control response")
    elif profile == "malformed":
        if text.count("malformed=1 duplicate=0") != 1:
            raise ValueError("malformed profile did not transmit exactly once")
        if "data=93080401a00802e091150101 " not in text or alerting != 1:
            raise ValueError("firmware did not produce the observed bounded malformed response")
    else:
        raise ValueError(f"unknown profile: {profile}")

    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if WAITING_FRAME not in hashes:
        raise ValueError("missing intact first-call/call-waiting presentation")
    return {"confirmations": confirmations, "alerting": alerting}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    parser.add_argument("profile", choices=("duplicate", "malformed"))
    args = parser.parse_args()
    try:
        result = verify(
            args.log.read_text(errors="replace"), args.frame_dir, args.profile)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        f"OK - {args.profile} call-waiting SETUP remained bounded "
        f"({result['confirmations']} confirmation, {result['alerting']} alerting)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
