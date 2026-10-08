#!/usr/bin/env python3
"""Check NHM-3 incoming-call signaling, not speech/audio fidelity."""

import argparse
import hashlib
from pathlib import Path
import sys
from PIL import Image

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools import radio_call_lifecycle_common as call
from tools import radio_outgoing_call_trace_check as outgoing


def check_frames(directory, *, outgoing_call=False):
    # Crop actual caller/digits and connected-call text, not interim animation.
    active = ((0, 8, 72, 24), 'cdb08190d86aa1c06f3297821158e128a330f140c6696a6fb9ea1b1ffd8cddec')
    idle = ((16, 0, 80, 16), '71ae5bf01fc32805f617bedf7a81e70c0183905c90d4921c7627ce137cb32e0a')
    frames = {
        '6250_call_1.png': ((48, 32, 96, 52), '6a142becccfac348a93b9fb805348aff2a82f8fed7fb8819d08218ee3927060c')
            if outgoing_call else ((16, 8, 88, 32), 'eafbb25a595f8461401184d389654ca51b6865a4ae12d94015da5d9ce9f2e5ec'),
        '6250_call_3.png' if outgoing_call else '6250_call_2.png': active,
        '6250_call_4.png' if outgoing_call else '6250_call_3.png': idle,
    }
    for name, (crop, expected) in frames.items():
        with Image.open(directory / name) as image:
            if image.size != (96, 60) or hashlib.sha256(
                    image.convert('L').crop(crop).tobytes()).hexdigest() != expected:
                raise ValueError('NHM-3 reviewed physical call pixels differ: ' + name)


def configured_radio_patterns():
    # Independently captured ARFCN19 SCH yields these channel parameters.
    return (
        r'TX packet type=02 payload=20 .*radio_phase=traffic_channel_change '
        r'data=041202000271012fc10000130000000400000000',
        r'TX packet type=02 payload=20 .*radio_phase=release_channel_change '
        r'data=041202001117001a600000130000001400000001',
    )


def verify(text: str, *, configured_carrier=False) -> None:
    traffic, release = configured_radio_patterns()
    call.require_ordered(text, (
        ("registration release", call.REGISTRATION_RELEASE),
        ("IMSI page", call.IMSI_PAGE),
        ("Paging Response", call.PAGING_RESPONSE),
        ("incoming SETUP", call.INCOMING_SETUP),
        ("Alerting", call.ALERTING),
        *((("configured traffic carrier 19", traffic),) if configured_carrier else ()),
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
         r"data=040000001117001a600000130000001400000001" if not configured_carrier else release),
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


def verify_outgoing(text: str, number: str = "123", *, configured_carrier=False) -> None:
    traffic, release = configured_radio_patterns()
    call.require_ordered(text, (
        ("physical Send", r"6250_call_input: step=7 pressed=1"),
        ("CM Service Request", outgoing.CM_SERVICE_REQUEST),
        ("CM Service Accept", outgoing.CM_SERVICE_ACCEPT),
        ("SETUP", outgoing.SETUP),
        ("Call Proceeding", outgoing.CALL_PROCEEDING),
        ("traffic assignment", outgoing.TRAFFIC_ASSIGNMENT),
        *((("configured traffic carrier 19", traffic),) if configured_carrier else ()),
        ("Assignment Complete", outgoing.ASSIGNMENT_COMPLETE),
        ("remote Alerting", outgoing.ALERTING),
        ("network Connect", outgoing.CONNECT),
        ("Connect Acknowledge", outgoing.CONNECT_ACKNOWLEDGE),
        ("physical End", r"6250_call_input: step=9 pressed=1"),
        ("Disconnect", outgoing.DISCONNECT),
        ("Release", outgoing.RELEASE),
        ("Release Complete", outgoing.RELEASE_COMPLETE),
        ("RR Channel Release", outgoing.RR_RELEASE),
        *((("configured release carrier 19", release),) if configured_carrier else ()),
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
    parser.add_argument('--configured-carrier', action='store_true')
    parser.add_argument('--frames', type=Path)
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors="replace")
        if args.outgoing:
            verify_outgoing(text, args.number, configured_carrier=args.configured_carrier)
        else:
            verify(text, configured_carrier=args.configured_carrier)
        if args.frames:
            check_frames(args.frames, outgoing_call=args.outgoing)
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: NHM-3 physical answer/end, release confirmation and idle PCH; audio unvalidated")


if __name__ == "__main__":
    main()
