#!/usr/bin/env python3
"""Summarize bounded organic MAD2 register traffic across supported ROMs."""

import argparse
import json
from collections import defaultdict
from pathlib import Path

try:
    from tools.mad2_access_census import parse
except ModuleNotFoundError:
    from mad2_access_census import parse


def analyze(label: str, path: Path) -> dict:
    accesses = [item for item in parse(path.read_text(errors="replace")) if item["bus"] == "IO"]
    values = defaultdict(set)
    pcs = defaultdict(set)
    for item in accesses:
        key = (item["direction"], item["offset"])
        values[key].add(item["data"])
        pcs[key].add(item["pc"])
    return {
        "label": label,
        "source": "organic two-second verbose boot log (generated, not retained)",
        "records": len(accesses),
        "registers": [
            {
                "direction": direction,
                "offset": offset,
                "values": sorted(values[(direction, offset)]),
                "pcs": [f"0x{pc:08x}" for pc in sorted(pcs[(direction, offset)])],
            }
            for direction, offset in sorted(values, key=lambda key: (key[1], key[0]))
        ],
    }


def build_payload(logs) -> dict:
    reports = [analyze(label, path) for label, path in logs]
    observed = sorted({item["offset"] for report in reports for item in report["registers"]})
    return {
        "schema_version": 1,
        "method": "bounded first-read/first-write MAD2 ledger from organic two-second boots",
        "limitations": "one record per direction/register/reset; values are first observations, not frequency counts",
        "coverage": {
            "roms": len(reports),
            "records": sum(report["records"] for report in reports),
            "observed_offsets": observed,
            "unobserved_offsets": sorted(set(range(0x100)) - set(observed)),
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
            raise SystemExit("checked runtime census requires all five supported ROM controls")
        required = {0x01, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0f, 0x10, 0x11, 0x12, 0x13}
        for report in payload["roms"]:
            observed = {item["offset"] for item in report["registers"]}
            missing = required - observed
            if missing:
                raise SystemExit(f"{report['label']} missing core boot offsets: " +
                                 ", ".join(f"0x{x:02x}" for x in sorted(missing)))
    output = json.dumps(payload, indent=2) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(output)
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
