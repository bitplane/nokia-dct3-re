#!/usr/bin/env python3
"""Verify host events after physical End cannot reopen or alter the call."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_outgoing_call_outcome_trace_check import NETWORK_DISCONNECT
from tools.radio_outgoing_call_trace_check import (
    CONNECT_ACKNOWLEDGE,
    DISCONNECT,
    PCH,
    RELEASE,
    RR_RELEASE,
)


TERMINATION_REJECTED = re.compile(
    r"gsm_call_adapter: termination id=1 cause=16 result=rejected"
)
MEDIA_REJECTED = re.compile(
    r"gsm_call_adapter: media direction=downlink id=1 sequence=0 "
    r"result=rejected"
)
ENDED = re.compile(
    r"gsm_call_adapter: state id=1 epoch=1 phase=ended"
)


def check(text: str) -> None:
    require_ordered(
        text,
        (
            ("handset Connect Acknowledge", CONNECT_ACKNOWLEDGE),
            ("physical End / Disconnect", DISCONNECT),
            ("network Release", RELEASE),
            ("RR Channel Release", RR_RELEASE),
            ("host-visible ended state", ENDED),
            ("post-release media rejection", MEDIA_REJECTED),
            ("post-release termination rejection", TERMINATION_REJECTED),
            ("return to PCH", PCH),
        ),
        "post-local-End host isolation",
    )
    if NETWORK_DISCONNECT.search(text):
        raise ValueError("stale host event produced a network Disconnect")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - physical End remained authoritative over stale host input")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
