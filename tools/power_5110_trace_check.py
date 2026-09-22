#!/usr/bin/env python3
"""Check NSE-1 short and sustained physical power-key behavior."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def verify(text: str, kind: str) -> None:
    press = re.search(r"input-press: .* name=power\b", text)
    release = re.search(r"input-release: .* name=power\b", text)
    off = re.search(r"ccont_power: event=off\b", text)
    if not press or not release or release.start() <= press.start():
        raise ValueError("physical power press/release was not observed")
    if off and off.start() <= press.start():
        raise ValueError("baseband rail dropped before the power press")
    if kind == "short" and off:
        raise ValueError("short power press removed the baseband rail")
    if kind == "long" and not off:
        raise ValueError("sustained power press did not remove the baseband rail")
    if "TMS320C54x" in text and "illegal" in text:
        raise ValueError("DSP executed an illegal instruction")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("short", "long"))
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.kind)
    except ValueError as error:
        raise SystemExit(f"FAIL - {error}") from None
    print(f"OK - NSE-1 physical {args.kind} power-key lifecycle reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
