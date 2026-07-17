#!/usr/bin/env python3
"""Check the firmware-owned short/long power-key lifecycle."""

import argparse
import sys
from pathlib import Path


def parse_summary(path: Path) -> dict[str, str]:
    values = {}
    for line in path.read_text(errors="replace").splitlines():
        key, separator, value = line.partition("=")
        if separator:
            values[key] = value
    return values


def check(kind: str, values: dict[str, str]) -> list[str]:
    errors = []
    mode = values.get("final_startup_mode")
    event = values.get("final_startup_event")
    sim_enable = values.get("final_sim_enable")
    modes = set(values.get("startup_modes", "").split(","))

    if kind == "short":
        if mode != "0004":
            errors.append(f"short press ended in mode {mode}, expected 0004")
        if "000C" in modes:
            errors.append("short press entered shutdown mode 000C")
        if sim_enable != "01":
            errors.append(f"short press changed SIM enable to {sim_enable}, expected 01")
    else:
        if mode != "000C":
            errors.append(f"long press ended in mode {mode}, expected 000C")
        if event != "0074":
            errors.append(f"long press ended with event {event}, expected 0074")
        if "000C" not in modes:
            errors.append("long press never entered shutdown mode 000C")
        if sim_enable != "00":
            errors.append(f"long press left SIM enabled ({sim_enable}), expected 00")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=("short", "long"))
    parser.add_argument("summary", type=Path)
    args = parser.parse_args()

    errors = check(args.kind, parse_summary(args.summary))
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"power lifecycle: {args.kind} press reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
