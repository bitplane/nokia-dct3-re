#!/usr/bin/env python3
import pathlib
import re
import sys


ALARM_RE = re.compile(r"ccont_rtc: event=alarm_write reg=(?P<reg>0[bc]) data=(?P<data>[0-9a-f]{2})")
MINUTE_RE = re.compile(
    r"ccont_rtc: event=second time=12:01:00 .*status=(?P<status>[0-9a-f]{2})"
)
ROUTE_RE = re.compile(
    r"ccont_route: state=1 irq_line=2 pending=(?P<pending>[0-9a-f]{3}) "
    r"mask=(?P<mask>[0-9a-f]{2})"
)
ACK_RE = re.compile(r"ccont_route: event=mad_ack data=(?P<data>[0-9a-f]{2})")


def check_trace(text: str) -> list[str]:
    errors = []
    alarm = {(m.group("reg"), m.group("data")) for m in ALARM_RE.finditer(text)}
    if ("0b", "01") not in alarm or ("0c", "0c") not in alarm:
        errors.append("missing 12:01 alarm programming through CCONT registers 0b/0c")

    minute = MINUTE_RE.search(text)
    if not minute or not (int(minute.group("status"), 16) & 0x80):
        errors.append("CCONT alarm source bit 7 was not latched at 12:01")

    route = ROUTE_RE.search(text)
    if not route:
        errors.append("CCONT alarm did not route to MAD2 IRQ2")
    elif not (int(route.group("pending"), 16) & 0x04):
        errors.append("MAD2 IRQ2 pending bit was absent")
    elif int(route.group("mask"), 16) & 0x04:
        errors.append("MAD2 IRQ2 was masked during alarm delivery")

    if not any(int(m.group("data"), 16) & 0x04 for m in ACK_RE.finditer(text)):
        errors.append("firmware did not acknowledge the MAD2 IRQ2 edge")
    return errors


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: ccont_rtc_trace_check.py MAME_ERROR_LOG")
    errors = check_trace(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    if errors:
        raise SystemExit("; ".join(errors))
    print("OK - CCONT RTC alarm and MAD2 IRQ2 delivery reproduced")


if __name__ == "__main__":
    main()
