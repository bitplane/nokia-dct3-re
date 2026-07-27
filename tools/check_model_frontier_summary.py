#!/usr/bin/env python3
"""Check model-independent runtime predicates for promoted handset frontiers."""

from __future__ import annotations

import argparse
from pathlib import Path


def read_summary(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key] = value
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("summary", type=Path)
    fiq0 = parser.add_mutually_exclusive_group()
    fiq0.add_argument("--require-fiq0", action="store_true")
    fiq0.add_argument("--reject-fiq0", action="store_true")
    parser.add_argument("--require-state-roundtrip", action="store_true")
    args = parser.parse_args()
    summary = read_summary(args.summary)

    failures: list[str] = []
    if summary.get("soft_resets") != "0":
        failures.append(f"soft_resets={summary.get('soft_resets', 'missing')}")
    try:
        fiq_seen = int(summary.get("fiq_seen", ""), 16)
    except ValueError:
        failures.append(f"fiq_seen={summary.get('fiq_seen', 'missing')}")
    else:
        # FIQ0 is the DSP-to-MCU receive boundary. The compact 0x74
        # service-control completion must traverse it organically.
        if args.require_fiq0 and not fiq_seen & 0x40:
            failures.append(f"FIQ0 not observed (fiq_seen={fiq_seen:02X})")
        if args.reject_fiq0 and fiq_seen & 0x40:
            failures.append(f"unexpected FIQ0 observed (fiq_seen={fiq_seen:02X})")
    if int(summary.get("lcd_full_dumps", "0")) == 0:
        failures.append("no complete LCD transfer observed")
    if args.require_state_roundtrip and summary.get("state_roundtrip") != "pass":
        failures.append(
            f"state_roundtrip={summary.get('state_roundtrip', 'missing')}"
        )

    if failures:
        print("frontier predicates failed: " + "; ".join(failures))
        return 1
    predicates = ["LCD activity", "reset"]
    if args.require_fiq0:
        predicates.insert(0, "DSP completion")
    if args.reject_fiq0:
        predicates.insert(0, "DSP completion absent")
    if args.require_state_roundtrip:
        predicates.append("save-state")
    print("OK - " + ", ".join(predicates) + " predicates pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
