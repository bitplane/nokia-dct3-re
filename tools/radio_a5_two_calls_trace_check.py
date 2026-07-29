#!/usr/bin/env python3
"""Prove cipher activation and release state do not leak across calls."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


ACTIVATION = re.compile(r"gsm_cipher: event=activated algorithm=1")
CLEAR = re.compile(r"gsm_cipher: event=cleared algorithm=1")
COMMAND = re.compile(r"data=8012[0-9a-f]*063501")
REQUEST = re.compile(r"GSM outgoing request id=(\d+)")
PCH = re.compile(r"PCH no-identity fill")


def check(text: str) -> list[str]:
    errors: list[str] = []
    requests = list(REQUEST.finditer(text))
    activations = list(ACTIVATION.finditer(text))
    clears = list(CLEAR.finditer(text))
    commands = list(COMMAND.finditer(text))
    if [match.group(1) for match in requests] != ["1", "2"]:
        errors.append("expected two distinct sequential outgoing requests")
        return errors
    if len(commands) != 2 or len(activations) != 2 or len(clears) != 2:
        errors.append("each call must activate and clear A5/1 exactly once")
        return errors
    between = PCH.search(text, requests[0].end(), commands[1].start())
    if not between:
        errors.append("first call did not return to PCH before the second")
    for index, command in enumerate(commands):
        request = requests[index]
        activation = activations[index]
        clear = clears[index]
        if not command.start() < activation.start() < request.start():
            errors.append(
                f"call {index + 1} did not order command, activation and request"
            )
        if not request.start() < clear.start():
            errors.append(f"call {index + 1} cleared cipher before its request")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    errors = check(args.log.read_text(errors="replace"))
    if errors:
        for error in errors:
            print(f"ERROR - {error}")
        return 1
    print("OK - sequential calls independently activated and cleared A5/1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
