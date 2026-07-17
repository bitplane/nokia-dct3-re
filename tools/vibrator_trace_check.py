#!/usr/bin/env python3
import pathlib
import re
import sys


EVENT = re.compile(r"vibrator: enabled=(?P<enabled>[01]) control=(?P<control>[0-9a-f]{2})")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: vibrator_trace_check.py MAME_ERROR_LOG")

    events = [match.groupdict() for match in EVENT.finditer(pathlib.Path(sys.argv[1]).read_text())]
    if not any(event == {"enabled": "1", "control": "55"} for event in events):
        raise SystemExit("missing vibrator enable edge with control 0x55")
    if not any(event == {"enabled": "0", "control": "55"} for event in events):
        raise SystemExit("missing vibrator disable edge with retained control 0x55")

    print("OK - MAD2 vibrator control, gate and disable edges reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
