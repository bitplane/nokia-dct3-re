#!/usr/bin/env python3
"""Verify one organic mobile-originated GSM speech-call lifecycle."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered


CM_SERVICE_REQUEST = re.compile(
    r"GSM service establish sapi=0 pd=05 message=24 length=(?P<length>\d+) "
    r"data=(?P<data>[0-9a-f]+)"
)
CM_SERVICE_ACCEPT = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=05 message=21 length=2"
)
SETUP = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=05 length=(?P<length>\d+) "
    r"data=(?P<data>[0-9a-f]+)"
)
CALL_PROCEEDING = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=03 message=02 length=2"
)
TRAFFIC_ASSIGNMENT = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=06 message=2e length=8"
)
ASSIGNMENT_COMPLETE = re.compile(
    r"GSM service uplink sapi=0 pd=06 message=29 length=3"
)
ALERTING = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=03 message=01 length=2"
)
CONNECT = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=03 message=07 length=2"
)
CONNECT_ACKNOWLEDGE = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=0f length=2"
)
DISCONNECT = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=25 length=\d+"
)
RELEASE = re.compile(
    r"GSM service downlink kind=\d+ sapi=0 pd=03 message=2d length=2"
)
RELEASE_COMPLETE = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=2a length=2"
)
RR_RELEASE = re.compile(r"LAPDm service Channel Release acknowledged")
SPEECH = re.compile(
    r"speech tick uplink=(?P<uplink>\d+) downlink=(?P<downlink>\d+).*"
    r"nonzero=(?P<up_nonzero>\d+)/(?P<down_nonzero>\d+)"
)
SPEECH_STOP = re.compile(r"speech stop control=040a")
PCH = re.compile(r"PCH no-identity fill")


def decode_called_digits(setup: bytes) -> str:
    try:
        index = setup.index(0x5E, 2)
    except ValueError as error:
        raise ValueError("SETUP omitted Called Party BCD Number") from error
    if index + 2 >= len(setup):
        raise ValueError("SETUP carried a truncated Called Party BCD Number")
    length = setup[index + 1]
    if length < 2 or index + 2 + length > len(setup):
        raise ValueError("SETUP carried an invalid Called Party BCD length")
    digits = []
    for octet in setup[index + 3:index + 2 + length]:
        for digit in (octet & 0x0F, octet >> 4):
            if digit == 0x0F:
                return "".join(digits)
            if digit > 9:
                raise ValueError("SETUP carried a non-decimal called digit")
            digits.append(str(digit))
    return "".join(digits)


def check(
        text: str,
        expected_number: str = "5551234",
        require_release_complete: bool = True) -> None:
    release_tail = (
        (("handset Release Complete", RELEASE_COMPLETE),)
        if require_release_complete else ()
    )
    require_ordered(
        text,
        (
            ("CM Service Request", CM_SERVICE_REQUEST),
            ("CM Service Accept", CM_SERVICE_ACCEPT),
            ("SETUP", SETUP),
            ("Call Proceeding", CALL_PROCEEDING),
            ("one traffic assignment", TRAFFIC_ASSIGNMENT),
            ("Assignment Complete", ASSIGNMENT_COMPLETE),
            ("remote Alerting", ALERTING),
            ("network Connect", CONNECT),
            ("handset Connect Acknowledge", CONNECT_ACKNOWLEDGE),
            ("physical End / Disconnect", DISCONNECT),
            ("network Release", RELEASE),
            *release_tail,
            ("RR Channel Release", RR_RELEASE),
            ("speech-route stop", SPEECH_STOP),
            ("return to PCH", PCH),
        ),
        "mobile-originated call",
    )
    if len(TRAFFIC_ASSIGNMENT.findall(text)) != 1:
        raise ValueError("mobile-originated call must contain exactly one traffic assignment")

    setup = SETUP.search(text)
    assert setup is not None
    setup_data = bytes.fromhex(setup.group("data"))
    if len(setup_data) != int(setup.group("length")):
        raise ValueError("SETUP trace length does not match its captured payload")
    number = decode_called_digits(setup_data)
    if number != expected_number:
        raise ValueError(f"SETUP called {number!r}, expected {expected_number!r}")

    service = CM_SERVICE_REQUEST.search(text)
    assert service is not None
    if len(bytes.fromhex(service.group("data"))) != int(service.group("length")):
        raise ValueError("CM Service Request trace length does not match its payload")

    connected = CONNECT_ACKNOWLEDGE.search(text)
    disconnected = DISCONNECT.search(text)
    assert connected is not None and disconnected is not None
    connected_media = [
        match for match in SPEECH.finditer(
            text, connected.end(), disconnected.start()
        )
        if int(match.group("down_nonzero")) > 0
    ]
    if not connected_media:
        raise ValueError("connected call carried no non-silent downlink speech")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--number", default="5551234")
    parser.add_argument(
        "--release-complete", choices=("required", "optional"),
        default="required",
        help="whether this product must publish CC Release Complete before RR release",
    )
    args = parser.parse_args()
    try:
        check(
            args.log.read_text(errors="replace"),
            args.number,
            args.release_complete == "required",
        )
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print(
        "OK - physical dial established one mobile-originated TCH/F call, "
        "carried non-silent speech and returned cleanly to PCH"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
