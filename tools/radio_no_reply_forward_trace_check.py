#!/usr/bin/env python3
"""Verify organic CFNRy registration and unanswered-call forwarding."""

import argparse
import pathlib
import re


EVENTS = (
    re.compile(r"gsm_ss: request=register transaction=1b invoke=1 service=2a "),
    re.compile(r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=39 "),
    re.compile(r"gsm_call_adapter: incoming state id=1 .* phase=paging "),
    re.compile(r"gsm_call_adapter: incoming state id=1 .* phase=alerting "),
    re.compile(r"gsm_ss: incoming call forwarded condition=no-reply destination_length=5 "),
    re.compile(r"gsm_call_adapter: incoming state id=1 .* phase=forwarded "),
)


def verify(text: str) -> None:
    cursor = 0
    for index, pattern in enumerate(EVENTS, 1):
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or misordered no-reply event {index}")
        cursor = match.end()
    registration = re.search(
        r"GSM service uplink sapi=0 pd=0b message=3b length=33 "
        r"data=[0-9a-f]*04012a83011084[0-9a-f]*8501057f0100 ", text)
    if not registration:
        raise ValueError("CFNRy request omitted speech scope or five-second timer")
    forwarded = re.search(
        r"gsm_ss: incoming call forwarded condition=no-reply", text)
    if not forwarded or not re.search(
            r"GSM service downlink .* pd=03 message=25 ", text[forwarded.end():]):
        raise ValueError("no-reply forwarding did not clear the handset call")
    if "incoming state id=1" in text[cursor:] and "phase=connected" in text[cursor:]:
        raise ValueError("forwarded call connected to the handset")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from None
    print("OK - organic speech CFNRy registration, alerting and timed forwarding")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
