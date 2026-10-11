#!/usr/bin/env python3
"""Verify GSM-FR cursor resynchronisation and stale media rejection on restore."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_outgoing_call_outcome_trace_check import (
    NETWORK_DISCONNECT,
    RR_RELEASE,
)
from tools.radio_outgoing_call_trace_check import PCH


ROUNDTRIP = re.compile(r"state_roundtrip: result=pass")
RESTORED_STATE = re.compile(
    r"gsm_call_adapter: state id=1 epoch=2 phase=connected "
    r"media_uplink_sequence=(?P<uplink>\d+) "
    r"media_downlink_sequence=(?P<downlink>\d+)"
)
STALE_MEDIA = re.compile(
    r"gsm_call_adapter: stale epoch=1 current=2 type=media "
    r"id=1 result=rejected"
)
ACCEPTED_MEDIA = re.compile(
    r"gsm_call_adapter: media direction=downlink id=1 "
    r"sequence=(?P<sequence>\d+) result=accepted"
)
TERMINATION = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=accepted"
)


def check(text: str, minimum_restored_frames: int = 80, *, physical_end: bool = False) -> None:
    release = (
        ('physical End', re.compile(r'5210_pin_state_physical: key=End(?:\s|$)')),
        ('handset Disconnect', re.compile(r'GSM service uplink sapi=0 pd=03 message=25')),
        ('network Release', re.compile(r'GSM service downlink kind=25 sapi=0 pd=03 message=2d')),
        ('handset Release Complete', re.compile(r'GSM service uplink sapi=0 pd=03 message=2a')),
    ) if physical_end else (
        ('host termination', TERMINATION), ('network Disconnect', NETWORK_DISCONNECT),
    )
    require_ordered(
        text,
        (
            ("save-state roundtrip", ROUNDTRIP),
            ("restored connected/media snapshot", RESTORED_STATE),
            ("stale pre-restore media rejection", STALE_MEDIA),
            ("restored media acceptance", ACCEPTED_MEDIA),
            *release,
            ("RR release", RR_RELEASE),
            *((('restored ended', re.compile(
                r'gsm_call_adapter: state id=1 epoch=2 phase=ended')),)
              if physical_end else ()),
            ("return to PCH", PCH),
        ),
        "host media restore",
    )
    state = RESTORED_STATE.search(text)
    assert state is not None
    end = (text.index('5210_pin_state_physical: key=End', state.end())
           if physical_end else len(text))
    accepted = [
        int(match.group("sequence"))
        for match in ACCEPTED_MEDIA.finditer(text, state.end(), end)
    ]
    expected = int(state.group("downlink"))
    if len(accepted) < minimum_restored_frames:
        raise ValueError("too few GSM-FR frames were accepted after restore")
    if accepted[:minimum_restored_frames] != list(
        range(expected, expected + minimum_restored_frames)
    ):
        raise ValueError("restored downlink sequence was not contiguous")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--frames", type=int, default=80)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"), args.frames)
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - restored host media resumed at the published saved cursor")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
