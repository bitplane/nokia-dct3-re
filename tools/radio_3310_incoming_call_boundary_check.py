#!/usr/bin/env python3
"""Verify the evidenced NHM-5 incoming-call frontier through CC Connect Ack."""

import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import (
    ALERTING,
    ASSIGNMENT_COMPLETE,
    CIPHER_MODE_COMPLETE,
    CONNECT,
    CONNECT_ACKNOWLEDGE,
    DISCONNECT,
    IDLE_PCH,
    IMSI_PAGE,
    INCOMING_SETUP,
    MM_INFORMATION,
    NETWORK_RELEASE,
    NO_CIPHER_COMMAND,
    PAGING_CONTENTION_UA,
    PAGING_RESPONSE,
    REGISTRATION_RELEASE,
    RELEASE_COMPLETE,
    RELEASE_CONFIRMATION,
    RR_CHANNEL_RELEASE,
    TRAFFIC_RELEASE_UA,
    TRAFFIC_SABM,
    TRAFFIC_UA,
    require_ordered,
)


CHECKPOINTS = (
    ("registration release", REGISTRATION_RELEASE),
    ("IMSI page", IMSI_PAGE),
    ("Paging Response", PAGING_RESPONSE),
    ("contention-resolution UA", PAGING_CONTENTION_UA),
    ("no-cipher Cipher Mode Command", NO_CIPHER_COMMAND),
    ("NHM-5 cipher-control publication", re.compile(
        r"TX packet type=14 payload=12 .*"
        r"data=001affffffffffffffff0000")),
    ("MM Information", MM_INFORMATION),
    ("Cipher Mode Complete", CIPHER_MODE_COMPLETE),
    ("incoming SETUP", INCOMING_SETUP),
    ("Call Confirmed", re.compile(
        r"GSM service uplink sapi=0 pd=03 message=08 length=11")),
    ("Alerting", ALERTING),
    ("traffic-channel configuration", re.compile(
        r"TX packet type=02 payload=20 .*data=041202000271012fc1")),
    ("traffic-main-link SABM", TRAFFIC_SABM),
    ("traffic-main-link UA", TRAFFIC_UA),
    ("Assignment Complete", ASSIGNMENT_COMPLETE),
)

TEARDOWN_CHECKPOINTS = (
    ("Disconnect", DISCONNECT),
    ("network Release", NETWORK_RELEASE),
    ("Release Complete", RELEASE_COMPLETE),
    ("RR Channel Release", RR_CHANNEL_RELEASE),
    ("traffic-link release UA", TRAFFIC_RELEASE_UA),
    ("NHM-5 release channel transaction", re.compile(
        r"TX packet type=02 payload=20 .*"
        r"data=041202001117001a6000005800000014")),
    ("release channel confirmation", RELEASE_CONFIRMATION),
    ("idle PCH schedule", IDLE_PCH),
)


def verify(text: str, answered: bool = False, ended: bool = False) -> None:
    cursor = require_ordered(text, CHECKPOINTS, "NHM-5")

    if answered or ended:
        connects = list(CONNECT.finditer(text, cursor))
        if not connects:
            raise ValueError("missing NHM-5 physical-answer checkpoint: Connect")
        if len(connects) != 1:
            raise ValueError(
                "NHM-5 retransmitted Connect before network acknowledgement")
        connect_acknowledge = CONNECT_ACKNOWLEDGE.search(
            text, connects[0].end())
        if not connect_acknowledge:
            raise ValueError(
                "missing NHM-5 network checkpoint: Connect Acknowledge")
        cursor = connect_acknowledge.end()

    if ended:
        require_ordered(text, TEARDOWN_CHECKPOINTS, "NHM-5 teardown", cursor)


def main() -> int:
    if len(sys.argv) not in (2, 3) or (
            len(sys.argv) == 3 and
            sys.argv[2] not in ("--answered", "--ended")):
        raise SystemExit(
            "usage: radio_3310_incoming_call_boundary_check.py "
            "MAME_ERROR_LOG [--answered|--ended]")
    mode = sys.argv[2] if len(sys.argv) == 3 else ""
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"),
               answered=mode == "--answered", ended=mode == "--ended")
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    suffix = {
        "--answered":
            " and physical Answer completed Connect/Connect Acknowledge",
        "--ended":
            " and physical End returned the radio to idle",
    }.get(mode, "")
    print("OK - NHM-5 organically completed traffic assignment" + suffix)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
