#!/usr/bin/env python3
"""Verify retained EF_LOCI use across a different-LAC cold boot."""

import argparse
import pathlib
import re

from radio_reselection_trace_check import require_after


LOCATION_UPDATE = re.compile(
    r"TX packet type=1b .*data=0080013f4905087000f110"
    r"(?P<old_lac>[0-9a-f]{4})")


def verify(
        text: str, serving_arfcn: int, neighbour_arfcn: int,
        allow_pre_update_invalidation: bool = False) -> None:
    requests = list(LOCATION_UPDATE.finditer(text))
    expected_second = ("0001", "fffe") if allow_pre_update_invalidation else (
        "0001",)
    if (len(requests) != 2 or
            requests[0].group("old_lac") != "0002" or
            requests[1].group("old_lac") not in expected_second):
        raise ValueError(
            "preserved cold boot must present retained cell-B LAC on cell A, "
            "then cell-A LAC or an evidenced invalidated location after "
            "organic reselection to B")

    selection = re.search(
        rf"serving cell selected old_arfcn={serving_arfcn} "
        rf"new_arfcn={neighbour_arfcn}", text)
    if not selection or not (
            requests[0].end() < selection.start() < requests[1].start()):
        raise ValueError(
            "Location Updates do not bracket the firmware-owned reselection")

    cursor = requests[0].end()
    cursor = require_after(
        text, r"LAPDm Location Updating Accept acknowledged", cursor,
        "cell-A Location Updating Accept")
    cursor = require_after(
        text, r"sim_device: update-binary fid=6f7e offset=4 length=5", cursor,
        "cell-A EF_LOCI update")
    cursor = require_after(
        text,
        rf"serving cell selected old_arfcn={serving_arfcn} "
        rf"new_arfcn={neighbour_arfcn}",
        cursor, "cell-B reselection")
    cursor = require_after(
        text, r"LAPDm Location Updating Accept acknowledged", cursor,
        "cell-B Location Updating Accept")
    cursor = require_after(
        text, r"sim_device: update-binary fid=6f7e offset=4 length=5", cursor,
        "cell-B EF_LOCI update")
    require_after(
        text,
        rf"RX enqueue type=80 payload=34 .*"
        rf"data=60[0-9a-f]{{10}}{neighbour_arfcn:04x}",
        cursor, "steady PCH on retained cell B")

    status_writes = list(re.finditer(
        r"sim_device: update-binary fid=6f7e offset=10 length=1", text))
    if status_writes and not allow_pre_update_invalidation:
        raise ValueError(
            "preserved registered status was rewritten during cold boot")
    if allow_pre_update_invalidation:
        if requests[1].group("old_lac") == "fffe" and not any(
                selection.end() < write.start() < requests[1].start()
                for write in status_writes):
            raise ValueError(
                "invalid old location was not preceded by firmware-owned "
                "EF_LOCI invalidation")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="verify retained-location different-LAC cold boot")
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--serving-arfcn", type=int, default=1)
    parser.add_argument("--neighbour-arfcn", type=int, default=2)
    parser.add_argument("--allow-pre-update-invalidation", action="store_true")
    args = parser.parse_args()
    try:
        verify(
            args.log.read_text(errors="replace"),
            args.serving_arfcn,
            args.neighbour_arfcn,
            args.allow_pre_update_invalidation,
        )
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - cold boot consumed retained cell-B LAC, updated on cell A, "
        "reselected and restored cell-B location organically")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
