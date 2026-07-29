#!/usr/bin/env python3
"""Verify sequential outgoing calls cannot share host state."""

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
from tools.radio_outgoing_call_trace_check import PCH


REQUEST_2 = re.compile(r"GSM outgoing request id=2 digits=5551234")
STALE_1 = re.compile(
    r"gsm_call_adapter: decision id=1 outcome=0 result=rejected")
ACCEPT_2 = re.compile(
    r"gsm_call_adapter: decision id=2 outcome=1 result=accepted")


def check(text):
    first_request = REQUEST.search(text)
    if not first_request:
        raise ValueError("missing first outgoing request")
    require_ordered(
        text,
        (
            ("first request", REQUEST),
            ("first busy Disconnect", NETWORK_DISCONNECT),
            ("first RR release", RR_RELEASE),
            ("first return to PCH", PCH),
            ("second request", REQUEST_2),
            ("old request-ID rejection", STALE_1),
            ("second accepted decision", ACCEPT_2),
            ("second busy Disconnect", NETWORK_DISCONNECT),
            ("second RR release", RR_RELEASE),
            ("second return to PCH", PCH),
        ),
        "two sequential host calls",
    )
    if len(NETWORK_DISCONNECT.findall(text)) != 2:
        raise ValueError("sequential busy calls did not clear exactly twice")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - sequential calls retained distinct request and teardown state")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
