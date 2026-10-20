#!/usr/bin/env python3
"""Check product-scoped MAD2 wake notification MMIO conformance."""
import argparse
from pathlib import Path
import re

PATTERN = re.compile(
    r"mad2_wake_report_fixture: phase=(\w+) machine=(\w+) configured=([01]) "
    r"irq=([0-9a-f]{2}) ctrl=([0-9a-f]{2}) t=([0-9.]+) mmio_conformance=1")
PRODUCTS = {"noki3310": 1, "noki3330": 1, "noki3210": 0}


def check(text, product):
    rows = PATTERN.findall(text)
    phases = ["awake_edge", "armed", "wake_edge", "ack_report", "blocked_request", "complete"]
    errors = []
    if [row[0] for row in rows] != phases:
        return ["missing, repeated or reordered fixture phases"]
    configured = PRODUCTS[product]
    expected = [1, 0, 9 if configured else 1, 1, 1, 0]
    for row, pending in zip(rows, expected):
        phase, machine, flag, irq, ctrl, _ = row
        if machine != product or int(flag) != configured:
            errors.append(f"{phase}: wrong product/configuration")
        if int(irq, 16) != pending:
            errors.append(f"{phase}: incorrect pending source ownership")
        if ctrl != "0a":
            errors.append(f"{phase}: global delivery changed")
    times = [float(row[5]) for row in rows]
    if times != sorted(times) or times[2] - times[1] < 0.049:
        errors.append("no ordered sleep interval before physical wake")
    if "LUA ERROR" in text:
        errors.append("Lua fixture failed")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--product", choices=PRODUCTS, required=True)
    args = parser.parse_args()
    errors = check(args.log.read_text(errors="replace"), args.product)
    if errors:
        parser.exit(1, "\n".join(errors) + "\n")
    print(f"PASS: {args.product} wake notification MMIO conformance")


if __name__ == "__main__":
    main()
