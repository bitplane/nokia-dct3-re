#!/usr/bin/env python3
"""Verify that active unconditional diversion routes before handset paging."""

import argparse
import pathlib


def verify(text):
    marker = "GSM incoming call forwarded before paging destination_length=5"
    if marker not in text:
        raise ValueError("missing network-owned forwarding decision")
    after = text.split(marker, 1)[1]
    forbidden = (
        "GSM incoming page",
        "incoming-call phase=alerting",
        "incoming-call phase=connected",
        "PUP buzzer enable=1",
    )
    present = [entry for entry in forbidden if entry in after]
    if present:
        raise ValueError(f"diverted call reached handset: {present!r}")
    if ("phase=forwarded reason=unconditional destination_length=5" not in
            after):
        raise ValueError("host forwarding outcome omitted routing metadata")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - forwarded incoming call produced no paging, alert or ringing")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
