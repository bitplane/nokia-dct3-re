#!/usr/bin/env python3
"""Check NHM-3 incoming-call signaling, not speech/audio fidelity."""

import argparse
from pathlib import Path
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools import radio_call_lifecycle_common as call
from tools import radio_outgoing_call_trace_check as outgoing


def verify(text: str) -> None:
    call.require_ordered(text, (
        ("registration release", call.REGISTRATION_RELEASE),
        ("IMSI page", call.IMSI_PAGE),
        ("Paging Response", call.PAGING_RESPONSE),
        ("incoming SETUP", call.INCOMING_SETUP),
        ("Alerting", call.ALERTING),
        ("Assignment Complete", call.ASSIGNMENT_COMPLETE),
        ("physical Send", r"6250_call_input: step=1 pressed=1"),
        ("Connect", call.CONNECT),
        ("Connect Acknowledge", call.CONNECT_ACKNOWLEDGE),
        ("physical End", r"6250_call_input: step=3 pressed=1"),
        ("Disconnect", call.DISCONNECT),
        ("network Release", call.NETWORK_RELEASE),
        ("Release Complete", call.RELEASE_COMPLETE),
        ("RR Channel Release", call.RR_CHANNEL_RELEASE),
        ("release deconfiguration request",
         r"TX packet type=02 .*radio_phase=release_channel_change "
         r"data=040000001117001a600000130000001400000001"),
        ("release confirmation", call.RELEASE_CONFIRMATION),
        ("firmware release consumer",
         r"6250_channel_confirmation: body=00 input=0409 expected=00 pending=00"),
        ("idle PCH", call.IDLE_PCH),
    ), "NHM-3")
    call.require_count(text, call.CONNECT, 1, "expected exactly one Connect")
    call.require_count(text, call.DISCONNECT, 1, "expected exactly one Disconnect")
    released = text.rfind("6250_channel_confirmation: body=00 input=0409 expected=00 pending=00")
    if "kind=speech" in text[released:]:
        raise ValueError("speech traffic continued after release confirmation")


def verify_outgoing(text: str, number: str = "123") -> None:
    call.require_ordered(text, (
        ("physical Send", r"6250_call_input: step=7 pressed=1"),
        ("CM Service Request", outgoing.CM_SERVICE_REQUEST),
        ("CM Service Accept", outgoing.CM_SERVICE_ACCEPT),
        ("SETUP", outgoing.SETUP),
        ("Call Proceeding", outgoing.CALL_PROCEEDING),
        ("traffic assignment", outgoing.TRAFFIC_ASSIGNMENT),
        ("Assignment Complete", outgoing.ASSIGNMENT_COMPLETE),
        ("remote Alerting", outgoing.ALERTING),
        ("network Connect", outgoing.CONNECT),
        ("Connect Acknowledge", outgoing.CONNECT_ACKNOWLEDGE),
        ("physical End", r"6250_call_input: step=9 pressed=1"),
        ("Disconnect", outgoing.DISCONNECT),
        ("Release", outgoing.RELEASE),
        ("Release Complete", outgoing.RELEASE_COMPLETE),
        ("RR Channel Release", outgoing.RR_RELEASE),
        ("release confirmation", call.RELEASE_CONFIRMATION),
        ("firmware release consumer",
         r"6250_channel_confirmation: body=00 input=0409 expected=00 pending=00"),
        ("idle PCH", call.IDLE_PCH),
    ), "NHM-3 outgoing")
    setup = outgoing.SETUP.search(text)
    data = bytes.fromhex(setup.group("data"))
    if len(data) != int(setup.group("length")):
        raise ValueError("SETUP length mismatch")
    if outgoing.decode_called_digits(data) != number:
        raise ValueError("SETUP did not contain the physically dialed number")
    call.require_count(text, outgoing.CONNECT_ACKNOWLEDGE, 1, "expected one Connect Acknowledge")
    call.require_count(text, outgoing.DISCONNECT, 1, "expected one Disconnect")
    released = text.rfind("6250_channel_confirmation: body=00 input=0409 expected=00 pending=00")
    if "kind=speech" in text[released:]:
        raise ValueError("speech traffic continued after release confirmation")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--outgoing", action="store_true")
    parser.add_argument("--number", default="123")
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors="replace")
        if args.outgoing:
            verify_outgoing(text, args.number)
        else:
            verify(text)
    except ValueError as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: NHM-3 physical answer/end, release confirmation and idle PCH; audio unvalidated")


if __name__ == "__main__":
    main()
