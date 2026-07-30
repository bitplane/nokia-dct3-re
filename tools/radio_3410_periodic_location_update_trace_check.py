#!/usr/bin/env python3
"""Verify NHM-2's finite DSP bootstrap and periodic Location Updating."""

import argparse
import pathlib
import re

try:
    from tools.radio_periodic_location_update_trace_check import verify
except ModuleNotFoundError:
    from radio_periodic_location_update_trace_check import verify


def check(text: str) -> None:
    verify(text)
    publications = re.findall(
        r"service code-block request=0001 event=initial", text)
    if len(publications) != 1:
        raise ValueError(
            f"expected one initial DSP code-block publication, observed "
            f"{len(publications)}")
    completions = len(re.findall(r"IRQ4 service-complete", text))
    if completions != 151:
        raise ValueError(
            f"NHM-2 code-block transfer did not terminate at 151 completions: "
            f"{completions}")
    if not re.search(r"RAM W off=0e2 data=0000", text):
        raise ValueError("firmware did not clear the completed code-block selector")
    if not re.search(r"RAM W off=0e4 data=0004", text):
        raise ValueError("firmware did not publish final code-block state four")
    reloads = [
        float(value) for value in re.findall(
            r"ccont_watchdog: event=reload data=31 t=([0-9.]+)", text)
    ]
    if not reloads or max(reloads) < 93.0:
        raise ValueError("NHM-2 did not sustain organic CCONT watchdog reloads")
    if "ccont_watchdog_expired" in text:
        raise ValueError("NHM-2 CCONT watchdog expired during long idle")
    if "unmapped program memory" in text:
        raise ValueError("NHM-2 entered invalid program memory during long idle")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        "OK - NHM-2 completed its finite DSP transfer and organic periodic "
        "Location Updating")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
