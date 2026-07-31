#!/usr/bin/env python3
"""Check independent sequential ordinary MT-SMS transactions on NSE-8."""

import pathlib
import re
import sys

try:
    from tools.radio_sms_acceptance_common import (
        FIRST_SMS_DELIVER_BODY,
        SECOND_SMS_DELIVER_BODY,
        sms_record,
    )
except ModuleNotFoundError:
    from radio_sms_acceptance_common import (
        FIRST_SMS_DELIVER_BODY,
        SECOND_SMS_DELIVER_BODY,
        sms_record,
    )


def verify(log: str, sim_nvram: bytes, state: bool = False) -> None:
    if state and "state_roundtrip: result=pass" not in log:
        raise ValueError("missing successful between-message state replay")
    if len(re.findall(r"PCH IMSI page transmitted channel=60", log)) != 2:
        raise ValueError("two ordinary messages did not use two pages")
    for reference in (0x40, 0x41):
        encoded = f"{reference:02x}"
        if not re.search(
                rf"GSM service uplink sapi=3 pd=09 message=01 length=5 "
                rf"data=89010202{encoded}", log):
            raise ValueError(f"missing handset RP-ACK for reference {encoded}")
    if len(re.findall(
            r"GSM service downlink kind=17 sapi=3 pd=09 message=04", log)) != 2:
        raise ValueError("both transactions did not receive network CP-ACK")
    if len(re.findall(
            r"LAPDm service Channel Release acknowledged nr=3", log)) != 2:
        raise ValueError("both transactions did not close their RR connection")

    first = sms_record(sim_nvram, 1)
    second = sms_record(sim_nvram, 2)
    if first[0] != 0x03 or not first[1:].startswith(FIRST_SMS_DELIVER_BODY):
        raise ValueError("record 1 is not the exact unread first message")
    if second[0] != 0x03 or not second[1:].startswith(SECOND_SMS_DELIVER_BODY):
        raise ValueError("record 2 is not the exact unread second message")


def main() -> int:
    if len(sys.argv) not in (3, 4):
        raise SystemExit(
            "usage: radio_sms_sequence_trace_check.py LOG SIM_NVRAM [state]")
    try:
        verify(
            pathlib.Path(sys.argv[1]).read_text(),
            pathlib.Path(sys.argv[2]).read_bytes(),
            len(sys.argv) == 4 and sys.argv[3] == "state")
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - two ordinary SMS messages completed isolated transactions")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
