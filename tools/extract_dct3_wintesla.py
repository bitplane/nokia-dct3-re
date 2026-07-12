#!/usr/bin/env python3
"""Extract contiguous raw flash regions from Nokia DCT3 Wintesla records."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


HEADER_SIZE = 9
PAYLOAD_SIZE = 0x2000
RECORD_SIZE = HEADER_SIZE + PAYLOAD_SIZE
RECORD_TYPE = 0x0B


def extract_records(path: Path) -> tuple[int, bytes]:
    source = path.read_bytes()
    if not source or len(source) % RECORD_SIZE:
        raise ValueError(f"{path}: size {len(source):#x} is not a Wintesla record stream")

    result = bytearray()
    first_address: int | None = None
    expected_address: int | None = None
    for offset in range(0, len(source), RECORD_SIZE):
        header = source[offset : offset + HEADER_SIZE]
        address = int.from_bytes(header[1:4], "big")
        length = int.from_bytes(header[5:8], "big")
        if header[0] != RECORD_TYPE or length != PAYLOAD_SIZE:
            raise ValueError(
                f"{path}: unsupported record at {offset:#x}: "
                f"type={header[0]:#x}, length={length:#x}"
            )
        if expected_address is not None and address != expected_address:
            raise ValueError(
                f"{path}: non-contiguous record at {offset:#x}: "
                f"expected {expected_address:#x}, got {address:#x}"
            )
        if first_address is None:
            first_address = address
        expected_address = address + length
        result.extend(source[offset + HEADER_SIZE : offset + RECORD_SIZE])

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
    parser.add_argument("--expect-flash-sha1")
    parser.add_argument("--expect-eeprom-sha1")
    args = parser.parse_args()

    mcu_address, mcu = extract_records(args.mcu)
    ppm_address, ppm = extract_records(args.ppm)
    pmm_address, pmm = extract_records(args.pmm)
    if mcu_address != 0x200000 or ppm_address != mcu_address + len(mcu):
        raise ValueError(
            f"MCU/PPM layout is not contiguous from 0x200000: "
            f"MCU={mcu_address:#x}+{len(mcu):#x}, PPM={ppm_address:#x}"
        )
    if pmm_address != 0x5F0000:
        raise ValueError(f"expected PMM at 0x5f0000, got {pmm_address:#x}")

    flash = mcu + ppm
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
    print(
        f"wrote {args.flash_output} ({len(flash):#x}, sha1 {flash_sha1})\n"
        f"wrote {args.eeprom_output} ({len(pmm):#x}, sha1 {eeprom_sha1})"
    )


if __name__ == "__main__":
    main()
