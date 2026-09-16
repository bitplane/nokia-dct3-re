#!/usr/bin/env python3
"""Check the physical NSM-5 short/long power-key boundary."""

from __future__ import annotations

import argparse
from pathlib import Path


def verify(text: str, kind: str) -> None:
    press = text.find("input-press:")
    release = text.find("input-release:")
    power_off = text.find("ccont_power: event=off")
    if press < 0 or release < press:
        raise ValueError("physical power-key press/release was not observed")
    if kind == "short" and power_off >= 0:
        raise ValueError("short power-key press removed the baseband rail")
    if kind == "long" and power_off < release:
        raise ValueError("long power-key press did not reach CCONT rail-off")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("short", "long"))
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.kind)
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from None
    print(f"OK - NSM-5 physical {args.kind} power-key lifecycle reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
