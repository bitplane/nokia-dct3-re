#!/usr/bin/env python3
"""Verify organic CFNRc registration and post-loss routing."""

import argparse
import pathlib
import re


EVENTS = (
    re.compile(r"gsm_ss: request=register transaction=1b invoke=1 service=2b "),
    re.compile(r"DOWNLINK_SIGNALLING_FAIL arfcn=1"),
    re.compile(r"GSM incoming call forwarded before paging "
               r"destination_length=5 condition=not-reachable "),
    re.compile(r"gsm_call_adapter: incoming state id=1 .* phase=forwarded "
               r"reason=not-reachable destination_length=5 "),
)


def verify(text):
    cursor = 0
    for index, pattern in enumerate(EVENTS, 1):
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(
                f"missing or misordered not-reachable event {index}")
        cursor = match.end()
    after = text[text.index("condition=not-reachable"):]
    if "GSM incoming page" in after:
        raise ValueError("not-reachable forwarded call reached handset paging")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from None
    print("OK - organic CFNRc routed a post-loss call before paging")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
