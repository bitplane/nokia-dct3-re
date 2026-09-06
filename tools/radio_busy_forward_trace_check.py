#!/usr/bin/env python3
"""Verify organic CFB registration and busy routing."""

import argparse
import pathlib
import re


EVENTS = (
    re.compile(r"gsm_ss: request=register transaction=1b invoke=1 service=29 "),
    re.compile(r"gsm_call_adapter: state id=1 .* phase=connected "),
    re.compile(r"GSM incoming call forwarded before paging "
               r"destination_length=5 condition=busy "),
    re.compile(r"gsm_call_adapter: incoming state id=2 .* phase=forwarded "
               r"reason=busy destination_length=5 "),
)


def verify(text):
    cursor = 0
    for index, pattern in enumerate(EVENTS, 1):
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or misordered busy-forwarding event {index}")
        cursor = match.end()
    after = text[text.index("condition=busy"):]
    for forbidden in ("GSM incoming page", "incoming-call phase=alerting"):
        if forbidden in after:
            raise ValueError(f"busy-forwarded call reached handset via {forbidden}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from None
    print("OK - organic speech CFB routed a second call before paging")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
