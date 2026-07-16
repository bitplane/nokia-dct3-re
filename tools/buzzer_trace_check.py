import pathlib
import re
import sys


EVENT = re.compile(
    r"buzzer: enabled=(?P<enabled>[01]) divider=(?P<divider>\d+) "
    r"frequency=(?P<frequency>\d+) volume=(?P<volume>\d+)"
)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: buzzer_trace_check.py MAME_ERROR_LOG")

    events = [match.groupdict() for match in EVENT.finditer(pathlib.Path(sys.argv[1]).read_text())]
    enabled = [event for event in events if event["enabled"] == "1"]
    if not any(
        event["divider"] == "6500"
        and event["frequency"] == "2000"
        and event["volume"] == "127"
        for event in enabled
    ):
        raise SystemExit("missing 2 kHz buzzer enable edge")
    if not any(event["enabled"] == "0" and event["divider"] == "6500" for event in events):
        raise SystemExit("missing buzzer disable edge")

    print("OK - MAD2 buzzer divider, gate and disable edges reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
