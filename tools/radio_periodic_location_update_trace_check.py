#!/usr/bin/env python3
"""Verify firmware-owned periodic Location Updating from broadcast T3212."""

import argparse
import pathlib
import re


SI3_T3212_ONE = re.compile(
    r"RX enqueue type=80 payload=34 .*data=[0-9a-f]{20}"
    r"49061b000100f1100001400001")
LOCATION_UPDATE = re.compile(
    r"GSM service establish sapi=0 pd=05 message=08 length=18 "
    r"data=0508(?P<kind>7[0-9a-f])[0-9a-f]+ t=(?P<time>[0-9.]+)")


def verify(text: str) -> None:
    if not SI3_T3212_ONE.search(text):
        raise ValueError("missing delivered SI3 with T3212=1")

    updates = list(LOCATION_UPDATE.finditer(text))
    if len(updates) != 2:
        raise ValueError(
            f"expected initial and periodic Location Updating, observed {len(updates)}")
    if updates[0]["kind"] != "70":
        raise ValueError("first Location Updating was not a normal update")
    if updates[1]["kind"] != "71":
        raise ValueError("second Location Updating was not a periodic update")

    elapsed = float(updates[1]["time"]) - float(updates[0]["time"])
    # T3212=1 is six minutes. This window rejects an immediate retry without
    # depending on one ROM's exact idle-task scheduling around timer expiry.
    if not 360.0 <= elapsed <= 450.0:
        raise ValueError(
            f"periodic Location Updating interval outside T3212 window: {elapsed:.3f}s")

    between = text[updates[0].end():updates[1].start()]
    if "LAPDm Location Updating Accept acknowledged nr=1" not in between:
        raise ValueError("initial Location Updating was not accepted")
    if "LAPDm Channel Release acknowledged nr=2" not in between:
        raise ValueError("initial RR channel was not cleanly released")
    if not re.search(r"TX packet type=0c .*radio_phase=random_access", between):
        raise ValueError("periodic update did not originate a new CHANNEL REQUEST")

    after = text[updates[1].end():]
    if "LAPDm Location Updating Accept acknowledged nr=1" not in after:
        raise ValueError("periodic Location Updating was not accepted")
    release = after.find("LAPDm Channel Release acknowledged nr=2")
    if release < 0:
        raise ValueError("periodic RR channel was not cleanly released")
    if not re.search(
            r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0",
            after[release:]):
        raise ValueError("handset did not return to steady PCH after periodic update")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="verify an organic T3212 periodic Location Updating lifecycle")
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - firmware honoured T3212, updated periodically, and returned to PCH")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
