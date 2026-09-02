#!/usr/bin/env python3
"""Check the externally originated incoming-call adapter lifecycle."""

import argparse
import pathlib
import re


def check(text: str, require_restore: bool = False) -> None:
    patterns = [
        r"gsm_call_adapter: incoming id=1 result=accepted",
        r"gsm_call_adapter: incoming state id=1 epoch=\d+ phase=queued",
        r"gsm_call_adapter: incoming state id=1 epoch=\d+ phase=paging",
        r"gsm_call_adapter: incoming state id=1 epoch=\d+ phase=alerting",
        r"gsm_call_adapter: incoming state id=1 epoch=\d+ phase=connected",
        r"gsm_call_adapter: media direction=uplink id=1 sequence=\d+ good=1",
        r"gsm_call_adapter: media direction=downlink id=1 sequence=\d+ result=accepted",
        r"gsm_call_adapter: termination id=1 cause=16 result=accepted",
        r"gsm_call_adapter: incoming state id=1 epoch=\d+ phase=ended",
    ]
    positions = []
    for pattern in patterns:
        match = re.search(pattern, text)
        if not match:
            raise ValueError(f"missing incoming host event: {pattern}")
        positions.append(match.start())
    if positions != sorted(positions):
        raise ValueError("incoming host lifecycle events were out of order")
    epochs = [int(value) for value in re.findall(
        r"incoming state id=1 epoch=(\d+) phase=connected", text)]
    if require_restore and len(set(epochs)) < 2:
        raise ValueError(
            "connected incoming call was not republished under a new epoch")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log")
    parser.add_argument("--require-restore", action="store_true")
    args = parser.parse_args()
    try:
        check(pathlib.Path(args.log).read_text(errors="replace"),
              args.require_restore)
    except ValueError as error:
        print(f"FAIL - {error}")
        return 1
    print("OK - external incoming host lifecycle is ordered and correlated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
