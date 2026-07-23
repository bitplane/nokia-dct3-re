#!/usr/bin/env python3
"""Summarize organic CCONT register traffic from verbose GENSIO logs."""

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path

try:
    from tools.gensio_trace_check import decode_transactions, parse_accesses
except ModuleNotFoundError:
    from gensio_trace_check import decode_transactions, parse_accesses


def analyze(label: str, path: Path) -> dict:
    accesses = parse_accesses(path.read_text(errors="replace"))
    errors, counts, transactions = decode_transactions(accesses)
    if errors:
        raise ValueError(f"{label}: " + "; ".join(errors))
    totals = Counter()
    values = defaultdict(set)
    for direction, register, value in transactions:
        totals[(direction, register)] += 1
        values[(direction, register)].add(value)
    adc_selectors = sorted({
        (value >> 4) & 0x07
        for direction, register, value in transactions
        if direction == "W" and register == 0
    })
    return {
        "label": label,
        "source": "organic verbose boot log (generated, not retained)",
        "gensio_accesses": counts["accesses"],
        "transactions": counts["reads"] + counts["writes"],
        "adc_selectors": adc_selectors,
        "registers": [
            {
                "direction": direction,
                "register": register,
                "count": totals[(direction, register)],
                "values": sorted(values[(direction, register)]),
            }
            for direction, register in sorted(totals, key=lambda item: (item[1], item[0]))
        ],
    }


def build_payload(logs) -> dict:
    reports = [analyze(label, path) for label, path in logs]
    observed = sorted({
        item["register"] for report in reports for item in report["registers"]
    })
    return {
        "schema_version": 1,
        "method": "organic two-second boot with verbose GENSIO transaction decoding",
        "coverage": {
            "roms": len(reports),
            "complete_transactions": sum(report["transactions"] for report in reports),
            "observed_registers": observed,
            "unobserved_registers": sorted(set(range(16)) - set(observed)),
        },
        "roms": reports,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", action="append", nargs=2, required=True, metavar=("LABEL", "PATH"))
    parser.add_argument("--json", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    payload = build_payload([(label, Path(path)) for label, path in args.log])
    if args.check:
        if payload["coverage"]["roms"] != 5:
            raise SystemExit("checked runtime census requires five ROM logs")
        required = {0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15}
        missing = required - set(payload["coverage"]["observed_registers"])
        if missing:
            raise SystemExit("boot register coverage changed: " + ", ".join(map(str, sorted(missing))))
    output = json.dumps(payload, indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
