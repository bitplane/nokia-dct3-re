#!/usr/bin/env python3
"""Check NHM-3 incoming-call signaling, not speech/audio fidelity."""

import argparse
from pathlib import Path
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools import radio_call_lifecycle_common as call


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


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: NHM-3 physical answer/end, release confirmation and idle PCH; audio unvalidated")


if __name__ == "__main__":
    main()
