#!/usr/bin/env python3
"""Verify physical calculator input and its reviewed 1+2=3 result frame."""
import argparse
from hashlib import sha256
from pathlib import Path
from PIL import Image

RESULT = "9ace2901a2bbf8b01669724c2cfaca1b4fb7669617e05b51852f6efce3546997"


def verify(log, pixels, size):
    cursor = 0
    for step in range(1, 29):
        event = f"6250_app_input: step={step} pressed={step % 2}"
        position = log.find(event, cursor)
        if position < 0:
            raise ValueError(f"missing ordered physical calculator input {step}")
        cursor = position + len(event)
    if size != (96, 60) or sha256(pixels).hexdigest() != RESULT:
        raise ValueError("missing reviewed calculator 1+2=3 frame")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("frame", type=Path)
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as frame:
            verify(args.log.read_text(), frame.convert("L").tobytes(), frame.size)
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print("PASS: NHM-3 physical calculator 1+2=3")


if __name__ == "__main__":
    main()
