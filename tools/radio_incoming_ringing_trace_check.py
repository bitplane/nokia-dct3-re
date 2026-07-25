#!/usr/bin/env python3
"""Verify organic incoming-call ringing through the MAD2 PUP buzzer."""

import pathlib
import re
import sys


PAGE_RE = re.compile(r"PCH IMSI page transmitted channel=60 fn=")
BUZZER_ON_RE = re.compile(
    r"buzzer: enabled=1 divider=(?P<divider>\d+) "
    r"frequency=(?P<frequency>\d+) volume=(?P<volume>\d+)")
CALL_CONFIRMED_RE = re.compile(
    r"GSM service uplink sapi=0 pd=03 message=08")
ALERTING_RE = re.compile(r"GSM service uplink sapi=0 pd=03 message=01")
ASSIGNMENT_COMPLETE_RE = re.compile(
    r"GSM service uplink sapi=0 pd=06 message=29")
BUZZER_OFF_RE = re.compile(
    r"buzzer: enabled=0 divider=0 frequency=0 volume=(?P<volume>\d+)")
CONNECT_RE = re.compile(r"GSM service uplink sapi=0 pd=03 message=07")


def verify(text: str) -> None:
    page = PAGE_RE.search(text)
    if not page:
        raise ValueError("missing incoming IMSI page")

    buzzer = next(
        (match for match in BUZZER_ON_RE.finditer(text, page.end())
         if int(match.group("divider")) > 0
         and int(match.group("frequency")) > 0
         and int(match.group("volume")) > 0),
        None)
    if not buzzer:
        raise ValueError("incoming call did not organically enable the MAD2 buzzer")

    cursor = buzzer.end()
    for label, pattern in (
            ("Call Confirmed", CALL_CONFIRMED_RE),
            ("Alerting", ALERTING_RE),
            ("Assignment Complete", ASSIGNMENT_COMPLETE_RE)):
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing ringing checkpoint: {label}")
        cursor = match.end()

    stopped = BUZZER_OFF_RE.search(text, cursor)
    if not stopped:
        raise ValueError("physical answer did not stop the MAD2 buzzer")
    if not CONNECT_RE.search(text, stopped.end()):
        raise ValueError("buzzer stopped without an organic CC Connect")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(
            "usage: radio_incoming_ringing_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - incoming page rang through MAD2 PUP and physical Answer "
        "stopped the buzzer")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
