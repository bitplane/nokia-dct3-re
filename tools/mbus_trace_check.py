#!/usr/bin/env python3
"""Check the bounded Nokia MAD2 MBUS controller trace."""

import argparse
import re
from pathlib import Path


ACCESS = re.compile(r"mbus: event=([RW]) off=([0-9a-f]{2}) data=([0-9a-f]{2}).*pc=([0-9a-f]{8})", re.I)


def parse(text: str) -> dict:
    accesses = []
    for match in ACCESS.finditer(text):
        accesses.append((match.group(1), int(match.group(2), 16),
                         int(match.group(3), 16), int(match.group(4), 16)))
    return {
        "accesses": accesses,
        "tx_bytes": len(re.findall(r"mbus: event=TX ", text)),
        "rx_ready": len(re.findall(r"mbus_device: event=rx_ready ", text)),
        "rx_reads": len(re.findall(r"mbus_device: event=rx_read data=a5 ", text)),
        "fixture_accepted": "mbus_fixture: data=a5 accepted=1" in text,
        "fiq2_ack": bool(re.search(r"mbus: event=W off=08 data=04 ", text)),
    }


def check_boot(result: dict) -> None:
    peripheral = [(direction, offset, data) for direction, offset, data, _ in result["accesses"]
                  if offset in (0x18, 0x19, 0x1A)]
    expected = [
        ("W", 0x18, 0x00), ("W", 0x18, 0x80), ("R", 0x18, 0x00),
        ("R", 0x18, 0x00), ("W", 0x18, 0x0C), ("R", 0x1A, 0x00),
        ("R", 0x19, 0xC0), ("W", 0x19, 0xC7), ("R", 0x18, 0x0C),
        ("W", 0x18, 0x4C),
    ]
    if peripheral[:len(expected)] != expected:
        raise SystemExit(f"unexpected MBUS initialization: {peripheral[:len(expected)]!r}")
    if result["tx_bytes"]:
        raise SystemExit("ordinary boot unexpectedly transmitted MBUS data")


def check_rx(result: dict) -> None:
    if not (result["fixture_accepted"] and result["rx_ready"] == 1 and
            result["rx_reads"] >= 1 and result["fiq2_ack"]):
        raise SystemExit(f"incomplete MBUS RX/FIQ2 lifecycle: {result!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("boot", "rx"))
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    result = parse(args.log.read_text(errors="replace"))
    (check_boot if args.mode == "boot" else check_rx)(result)
    print(f"MBUS {args.mode} contract: accesses={len(result['accesses'])} "
          f"tx={result['tx_bytes']} rx_ready={result['rx_ready']} rx_reads={result['rx_reads']}")


if __name__ == "__main__":
    main()
