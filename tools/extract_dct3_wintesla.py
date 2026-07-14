#!/usr/bin/env python3
"""Extract contiguous raw flash regions from Nokia DCT3 Wintesla records."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


HEADER_SIZE = 9
PAYLOAD_SIZE = 0x2000
RECORD_TYPE = 0x0B


def extract_records(path: Path) -> tuple[int, bytes]:
    source = path.read_bytes()
    if not source:
        raise ValueError(f"{path}: empty Wintesla record stream")

    result = bytearray()
    first_address: int | None = None
    expected_address: int | None = None
    offset = 0
    while offset < len(source):
        if len(source) - offset < HEADER_SIZE:
            raise ValueError(f"{path}: truncated header at {offset:#x}")
        header = source[offset : offset + HEADER_SIZE]
        address = int.from_bytes(header[1:4], "big")
        length = int.from_bytes(header[5:8], "big")
        if header[0] != RECORD_TYPE or not 0 < length <= PAYLOAD_SIZE:
            raise ValueError(
                f"{path}: unsupported record at {offset:#x}: "
                f"type={header[0]:#x}, length={length:#x}"
            )
        record_end = offset + HEADER_SIZE + length
        if record_end > len(source):
            raise ValueError(
                f"{path}: truncated payload at {offset:#x}: "
                f"length={length:#x}, remaining={len(source) - offset - HEADER_SIZE:#x}"
            )
        if expected_address is not None and address != expected_address:
            raise ValueError(
                f"{path}: non-contiguous record at {offset:#x}: "
                f"expected {expected_address:#x}, got {address:#x}"
            )
        if first_address is None:
            first_address = address
        expected_address = address + length
        result.extend(source[offset + HEADER_SIZE : record_end])
        offset = record_end

    assert first_address is not None
    return first_address, bytes(result)


def digest(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mcu", type=Path, required=True)
    parser.add_argument("--ppm", type=Path, required=True)
    parser.add_argument("--pmm", type=Path, required=True)
    parser.add_argument("--flash-output", type=Path, required=True)
    parser.add_argument("--eeprom-output", type=Path, required=True)
    parser.add_argument("--combined-output", type=Path)
    parser.add_argument("--expect-flash-sha1")
    parser.add_argument("--expect-eeprom-sha1")
    args = parser.parse_args()

    mcu_address, mcu = extract_records(args.mcu)
    ppm_address, ppm = extract_records(args.ppm)
    pmm_address, pmm = extract_records(args.pmm)
    mcu_end = mcu_address + len(mcu)
    if mcu_address != 0x200000 or ppm_address < mcu_end:
        raise ValueError(
            f"MCU/PPM layout is invalid from 0x200000: "
            f"MCU={mcu_address:#x}+{len(mcu):#x}, PPM={ppm_address:#x}"
        )
    ppm_end = ppm_address + len(ppm)
    if pmm_address < ppm_end:
        raise ValueError(
            f"PMM overlaps MCU/PPM: PPM={ppm_address:#x}+{len(ppm):#x}, "
            f"PMM={pmm_address:#x}"
        )

    flash = mcu + (b"\xff" * (ppm_address - mcu_end)) + ppm
    flash_sha1 = digest(flash)
    eeprom_sha1 = digest(pmm)
    if args.expect_flash_sha1 and flash_sha1 != args.expect_flash_sha1.lower():
        raise ValueError(f"flash SHA-1 mismatch: expected {args.expect_flash_sha1}, got {flash_sha1}")
    if args.expect_eeprom_sha1 and eeprom_sha1 != args.expect_eeprom_sha1.lower():
        raise ValueError(f"EEPROM SHA-1 mismatch: expected {args.expect_eeprom_sha1}, got {eeprom_sha1}")
    args.flash_output.parent.mkdir(parents=True, exist_ok=True)
    args.eeprom_output.parent.mkdir(parents=True, exist_ok=True)
    args.flash_output.write_bytes(flash)
    args.eeprom_output.write_bytes(pmm)
    summary = (
        f"wrote {args.flash_output} ({len(flash):#x}, sha1 {flash_sha1})\n"
        f"wrote {args.eeprom_output} ({len(pmm):#x}, sha1 {eeprom_sha1})"
    )
    if args.combined_output:
        combined = flash + (b"\xff" * (pmm_address - ppm_end)) + pmm
        args.combined_output.parent.mkdir(parents=True, exist_ok=True)
        args.combined_output.write_bytes(combined)
        summary += (
            f"\nwrote {args.combined_output} ({len(combined):#x}, "
            f"sha1 {digest(combined)})"
        )
    print(summary)


if __name__ == "__main__":
    main()
