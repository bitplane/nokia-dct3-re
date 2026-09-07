#!/usr/bin/env python3
"""Check the unattended DISPLAY TEXT timeout lifecycle."""

import argparse
import re
from pathlib import Path


FETCH = re.compile(r"header cla=a0 ins=12 .* t=([0-9.]+)")
RESPONSE = re.compile(r"body ins=14 .* t=([0-9.]+)")


def verify(log: str) -> None:
    fetch_time = None
    response_time = None
    result_seen = False
    for raw_line in log.splitlines():
        line = raw_line.replace("[:sim_card] ", "")
        if fetch_time is None and (match := FETCH.search(line)):
            fetch_time = float(match.group(1))
        elif (fetch_time is not None and response_time is None and
                (match := RESPONSE.search(line))):
            response_time = float(match.group(1))
        elif response_time is not None and "terminal-response data=" in line:
            payload = line.split("terminal-response data=", 1)[1].replace(" ", "")
            result_seen = payload.startswith("810301218002028281030112")
            break
    if fetch_time is None or response_time is None or not result_seen:
        raise ValueError("missing FETCH or result-12 terminal response")
    delay = response_time - fetch_time
    if not 55.0 <= delay <= 65.0:
        raise ValueError(f"unexpected firmware timeout {delay:.3f}s")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"))
    except ValueError as error:
        raise SystemExit(f"SIM TOOLKIT TIMEOUT: FAIL: {error}") from error
    print("SIM TOOLKIT TIMEOUT: PASS firmware returned no-user-response")


if __name__ == "__main__":
    main()
