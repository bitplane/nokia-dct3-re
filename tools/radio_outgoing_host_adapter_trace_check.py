#!/usr/bin/env python3
"""Verify the WebSocket call adapter stayed outside the GSM state machine."""

import argparse
import pathlib
import re
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_outgoing_call_outcome_trace_check import (
    NETWORK_DISCONNECT,
    REQUEST,
    RR_RELEASE,
)


PUBLISHED = re.compile(
    r"gsm_call_adapter: request id=1 epoch=1 digits=5551234 clients=1"
)
WRONG_ID = re.compile(
    r"gsm_call_adapter: decision id=2 outcome=1 result=rejected"
)
ACCEPTED = re.compile(
    r"gsm_call_adapter: decision id=1 outcome=1 result=accepted"
)
DUPLICATE = re.compile(
    r"gsm_call_adapter: decision id=1 outcome=1 result=rejected"
)
FALLBACK = re.compile(r"outgoing decision queued id=1 outcome=0")


def check(text: str) -> None:
    require_ordered(
        text,
        (
            ("saved outgoing request", REQUEST),
            ("host request publication", PUBLISHED),
            ("wrong request-ID rejection", WRONG_ID),
            ("one accepted host decision", ACCEPTED),
            ("duplicate decision rejection", DUPLICATE),
            ("network busy Disconnect", NETWORK_DISCONNECT),
            ("RR Channel Release", RR_RELEASE),
        ),
        "host outgoing-call adapter",
    )
    if len(ACCEPTED.findall(text)) != 1:
        raise ValueError("host adapter must accept exactly one decision")
    if FALLBACK.search(text):
        raise ValueError("deterministic fallback ran while the host adapter was enabled")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - host decision was ID-correlated and entered only through the session queue")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
