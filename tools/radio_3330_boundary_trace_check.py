#!/usr/bin/env python3
"""Verify the evidenced NHM-6 DCS serving-cell acceptance boundary."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("candidate window", re.compile(
        r"TX packet type=56 payload=160 .*data=0337f+")),
    ("serving channel change", re.compile(
        r"TX packet type=02 payload=20 .*data=041202000000005050000337")),
    ("SI4 parser", re.compile(
        r"nhm6_bcch_parse: channel=50 .*data=31061c00f1100001")),
    ("SI2 parser", re.compile(
        r"nhm6_bcch_parse: channel=50 .*data=59061a")),
    ("SI3 parser", re.compile(
        r"nhm6_bcch_parse: channel=50 .*data=49061b000100f1100001")),
    ("DCS SI1 parser", re.compile(
        r"nhm6_bcch_parse: channel=50 .*data=5906198f9b80")),
    ("SI1 terminal", re.compile(
        r"nhm6_si_terminal: message=19 changed=[0-9a-f]{2} ")),
    ("common-control channel", re.compile(
        r"TX packet type=02 payload=20 .*data=041202090000001060000337")),
    ("SI4 selector publication", re.compile(
        r"radio_si_selector_insert: firmware=nhm6 .*type=03ec")),
    ("accepted SI4 selection", re.compile(
        r"radio_si_after_advance: firmware=nhm6 .*selected_type=03ec")),
    ("automatic-access publication", re.compile(
        r"TX packet type=46 payload=8 .*data=3210321000010000")),
)


def verify(text: str) -> None:
    # Multi-call LOGMASKED byte dumps acquire the default root-device prefix
    # between bytes. Remove only that presentation artifact.
    text = text.replace("[:] ", "")
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or out-of-order NHM-6 boundary checkpoint: {label}")
        cursor = match.end()

    if re.search(r"TX packet type=57 payload=4 .*data=03050000", text):
        raise ValueError(
            "NHM-6 regressed to the incomplete-SI post-confirmation path")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_3330_boundary_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        print(error, file=sys.stderr)
        return 1
    print(
        "OK - NHM-6 parsed standards-shaped DCS SI, selected SI4, and "
        "published automatic access")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
