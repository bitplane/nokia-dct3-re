#!/usr/bin/env python3
"""Check that a late NSE-1 input follows an awake ROM4 clock state."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


CLOCK = re.compile(r"mad2_clock_ctrl: data=([0-9a-f]{2}).* t=([0-9.]+)")
PRESS = re.compile(r"input-press: t=([0-9.]+) name=menu\b")


def verify(text: str) -> None:
    press = PRESS.search(text)
    release = re.search(r"input-release: .* name=menu\b", text)
    if not press or not release or release.start() <= press.start():
        raise ValueError("late physical Menu press/release was not observed")
    press_time = float(press.group(1))
    if press_time < 10:
        raise ValueError("Menu press did not exercise the later idle period")
    writes = [(int(match.group(1), 16), float(match.group(2)))
              for match in CLOCK.finditer(text)]
    if not writes:
        raise ValueError("MAD2 clock-control writes were not traced")
    if any(data & 0x02 for data, time in writes if time <= press_time):
        raise ValueError("firmware requested clock stop before the Menu press")
    if "mad2_sleep: event=request " in text:
        raise ValueError("ARM suspended during the late-input fixture")
    if "rom4_reset_request" in text:
        raise ValueError("ROM4 baseband reset during late input")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from None
    print("OK - NSE-1 later idle interval remains clocked and accepts physical Menu input")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
