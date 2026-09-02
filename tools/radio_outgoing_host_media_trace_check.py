#!/usr/bin/env python3
"""Verify host GSM-FR media remains correlated and crosses the radio path."""

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
from tools.radio_outgoing_call_trace_check import CONNECT_ACKNOWLEDGE, PCH


CONNECTED = re.compile(
    r"gsm_call_adapter: state id=1 epoch=1 phase=connected")
UPLINK = re.compile(
    r"gsm_call_adapter: media direction=uplink id=1 sequence=\d+ good=1")
ERASED_UPLINK = re.compile(
    r"gsm_call_adapter: media direction=uplink id=1 sequence=\d+ good=0")
DOWNLINK = re.compile(
    r"gsm_call_adapter: media direction=downlink id=1 sequence=(?P<seq>\d+) "
    r"result=accepted")
TERMINATION = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=accepted")
LATE_MEDIA = re.compile(
    r"gsm_call_adapter: media direction=downlink id=1 sequence=200 "
    r"result=rejected")


def check(text: str, frames: int = 8, standalone: bool = False) -> None:
    checkpoints = [
        ("connected publication", CONNECTED),
        ("FACCH/BFI erased uplink publication", ERASED_UPLINK),
        ("decoded uplink GSM-FR publication", UPLINK),
        ("first accepted host downlink GSM-FR frame", DOWNLINK),
        ("host termination", TERMINATION),
        ("network Disconnect", NETWORK_DISCONNECT),
        ("RR Channel Release", RR_RELEASE),
    ]
    if not standalone:
        checkpoints.append(("late media rejection", LATE_MEDIA))
    checkpoints.append(("return to PCH", PCH))
    require_ordered(
        text,
        checkpoints,
        "host GSM-FR media",
    )
    sequences = [int(match.group("seq")) for match in DOWNLINK.finditer(text)]
    expected = list(range(len(sequences) if standalone else frames))
    if sequences != expected or (standalone and len(sequences) < frames):
        raise ValueError(
            f"host downlink sequences were {sequences!r}, "
            f"expected {'at least ' if standalone else ''}{frames} contiguous frames")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--frames", type=int, default=8)
    parser.add_argument("--standalone", action="store_true")
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"), args.frames, args.standalone)
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - correlated host GSM-FR crossed decoded uplink and TCH/F downlink")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
