#!/usr/bin/env python3
"""Check that flash-backed PMM changed without modifying firmware."""

import argparse
from pathlib import Path


def check(nvram: bytes, firmware: bytes, virgin_pmm: bytes) -> list[str]:
    errors = []
    expected_size = len(firmware) + len(virgin_pmm)
    if len(nvram) != expected_size:
        return [f"flash NVRAM size {len(nvram):#x}, expected {expected_size:#x}"]
    if nvram[:len(firmware)] != firmware:
        errors.append("firmware prefix changed while updating permanent memory")
    changed = sum(a != b for a, b in zip(nvram[len(firmware):], virgin_pmm))
    if not changed:
        errors.append("flash-backed PMM is still identical to the virgin image")
    return errors


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("nvram", type=Path)
    parser.add_argument("firmware", type=Path)
    parser.add_argument("virgin_pmm", type=Path)
    args = parser.parse_args()
    nvram = args.nvram.read_bytes()
    firmware = args.firmware.read_bytes()
    virgin_pmm = args.virgin_pmm.read_bytes()
    errors = check(nvram, firmware, virgin_pmm)
    if errors:
        raise SystemExit("flash PMM persistence failed: " + "; ".join(errors))
    changed = sum(
        a != b for a, b in zip(nvram[len(firmware):], virgin_pmm))
    print(
        "flash PMM persistence: firmware unchanged; "
        f"{changed} permanent-memory bytes differ from virgin input")


if __name__ == "__main__":
    main()
