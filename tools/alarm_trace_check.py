#!/usr/bin/env python3
import pathlib
import re
import sys


PROGRAM_MINUTE_RE = re.compile(
    r"ccont_rtc: event=alarm_write reg=0b data=02 armed=[01] t=(?P<time>[0-9.]+)"
)
PROGRAM_HOUR_RE = re.compile(
    r"ccont_rtc: event=alarm_write reg=0c data=0c armed=1 t=(?P<time>[0-9.]+)"
)
MATCH_RE = re.compile(
    r"ccont_rtc: event=second time=12:02:\d\d .*status=(?P<status>[0-9a-f]{2}) .*t=(?P<time>[0-9.]+)"
)
DEADLINE_CLEAR_RE = re.compile(
    r"rtc_alarm_route: map-output=06bc .*deadline=7fffffff .*t=(?P<time>[0-9.]+)"
)
BUZZER_RE = re.compile(
    r"buzzer: enabled=1 divider=(?P<divider>\d+) frequency=(?P<frequency>\d+) "
    r"volume=(?P<volume>\d+) t=(?P<time>[0-9.]+)"
)


def check_trace(text: str) -> list[str]:
    errors = []
    minute = PROGRAM_MINUTE_RE.search(text)
    hour = PROGRAM_HOUR_RE.search(text)
    if not minute or not hour:
        errors.append("firmware did not program the keypad-selected 12:02 CCONT alarm")

    match = next(
        (candidate for candidate in MATCH_RE.finditer(text)
         if int(candidate.group("status"), 16) & 0x80),
        None,
    )
    if not match:
        errors.append("CCONT did not latch alarm source bit 7 at 12:02")

    cleared = None
    if match:
        match_time = float(match.group("time"))
        cleared = next(
            (candidate for candidate in DEADLINE_CLEAR_RE.finditer(text)
             if float(candidate.group("time")) > match_time),
            None,
        )
        if not cleared:
            errors.append("firmware did not consume the alarm and clear its deadline")

    buzzer = None
    if cleared:
        clear_time = float(cleared.group("time"))
        buzzer = next(
            (candidate for candidate in BUZZER_RE.finditer(text)
             if float(candidate.group("time")) > clear_time
             and int(candidate.group("divider")) > 0
             and int(candidate.group("volume")) > 0),
            None,
        )
        if not buzzer:
            errors.append("organic alarm handling did not start the MAD2 buzzer")
    return errors


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: alarm_trace_check.py MAME_ERROR_LOG")
    errors = check_trace(pathlib.Path(sys.argv[1]).read_text(errors="replace"))
    if errors:
        raise SystemExit("; ".join(errors))
    print("OK - organic keypad alarm programmed, fired and rang through CCONT/MAD2")


if __name__ == "__main__":
    main()
