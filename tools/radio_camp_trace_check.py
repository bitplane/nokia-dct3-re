#!/usr/bin/env python3
import pathlib
import re
import sys


CHECKPOINTS = (
    ("usable candidate", re.compile(r"radio_candidate_table: .*usable=01 arfcn=0001 ")),
    ("NO_PSW_LEFT acceptance", re.compile(r"radio_no_psw_left_decode: .*accept=03 ")),
    (
        "channel-change acceptance",
        re.compile(r"radio_channel_change_state: pc=00284f74 accept=03 controller=02 "),
    ),
    (
        "task-11 acquisition action",
        re.compile(
            r"radio_acquisition_transition: pc=00213c04 .*status=13a5 "
            r"action=01 arg=01 .*controller=03 "
        ),
    ),
    (
        "SI3 identity",
        re.compile(
            r"radio_bcch_parse: channel=50 .*data=.*49.*06.*1b.*00.*01.*32.*f4.*51.*00.*01"
        ),
    ),
    (
        "SI1-SI4 bitmap",
        re.compile(r"radio_si3_path: pc=00273e58 .*flags=0f/0f "),
    ),
)


def verify(text: str) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order radio camp checkpoint: {label}")
        cursor = match.end()


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: radio_camp_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - usable ARFCN 1 selected and serving-cell SI1-SI4 accepted organically")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
