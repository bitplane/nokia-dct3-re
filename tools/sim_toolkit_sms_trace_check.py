#!/usr/bin/env python3
"""Check organic SIM Toolkit SEND SHORT MESSAGE through GSM transport."""

import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.sim_toolkit_trace_check import require_in_order


def verify(log: str, *, cp=0x29, message_reference=0) -> None:
    if cp not in (0x29, 0x39) or message_reference not in (0, 1):
        raise ValueError('unsupported observed Toolkit SMS fixture identifiers')
    compact = log.replace("[:sim_card] ", "")
    require_in_order(compact, [
        "envelope data=d30702020181100101",
        "menu selection item=1 accepted",
        "proactive SEND SHORT MESSAGE ready",
        "SIM status ins=c2 sw=9124",
        "header cla=a0 ins=12 p1=00 p2=00 p3=24",
        "GSM service uplink sapi=3 pd=09 message=01 length=28 "
        f"data={cp:02x}0119000100069121436587090e01{message_reference:02x}0781551532f4000403534154",
        f"gsm_sms_submit: cp={cp:02x} rp=01 smsc=1234567890 "
        "destination=5551234 alphabet=1 user_length=3 outcome=0 status_report=0",
        "GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2",
        "GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5",
        "terminal-response data=810305130002028281030100",
        "SIM status ins=14 sw=9000",
        f"GSM service uplink sapi=3 pd=09 message=04 length=2 data={cp:02x}04",
        "LAPDm service Channel Release acknowledged nr=2",
    ])
    if compact.count("gsm_sms_submit:") != 1:
        raise ValueError("expected exactly one decoded toolkit SMS-SUBMIT")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT SMS: FAIL: {error}") from error
    print("SIM TOOLKIT SMS: PASS proactive command completed through CP/RP/RR")


if __name__ == "__main__":
    main()
