#!/usr/bin/env python3
"""Validate the recovered 3210 display-profile and LCD transport boundaries."""

import argparse
from pathlib import Path
import re
import sys


FLASH_BASE = 0x200000
DESCRIPTORS = {
    "v600": (0x2DAD78, 0x18A8),
    "v501": (0x2CF510, 0x18A0),
}

PROFILE_RE = re.compile(
    r"display_profile: pc=([0-9a-fA-F]{8}) "
    r"source0=([0-9a-fA-F]{24}) active=([0-9a-fA-F]{20})"
)
IO_RE = re.compile(
    r"display_io: off=([0-9a-fA-F]{2}) data=([0-9a-fA-F]{2})"
)


def effective_u32(image: bytes, address: int) -> int:
    offset = address - FLASH_BASE
    raw = int.from_bytes(image[offset:offset + 4], "little")
    return ((raw & 0xFFFF) << 16) | (raw >> 16)


def check_descriptor(rom: bytes, eeprom: bytes, firmware: str):
    errors = []
    address, expected_offset = DESCRIPTORS[firmware]
    key = effective_u32(rom, address)
    location_length = effective_u32(rom, address + 4)
    offset = location_length >> 16
    length = location_length & 0xFFFF
    if key != 0x0749:
        errors.append(f"descriptor key is 0x{key:04x}, expected 0x0749")
    if offset != expected_offset:
        errors.append(
            f"descriptor offset is 0x{offset:04x}, expected 0x{expected_offset:04x}"
        )
    if length != 12:
        errors.append(f"descriptor record length is {length}, expected 12")
    records = eeprom[offset:offset + length * 3]
    if len(records) != length * 3:
        errors.append("EEPROM is too short for three display-profile records")
    erased = bool(records) and all(value == 0xFF for value in records)
    return errors, {"offset": offset, "length": length, "erased": erased}


def parse_profile(text: str):
    return {
        int(pc, 16): (bytes.fromhex(source), bytes.fromhex(active))
        for pc, source, active in PROFILE_RE.findall(text)
    }


def check_v600_profile(text: str):
    errors = []
    profiles = parse_profile(text)
    for pc in (0x29A996, 0x29A768, 0x2B1E80):
        if pc not in profiles:
            errors.append(f"missing display-profile boundary 0x{pc:08x}")
    expected = bytearray([0xFF]) * 12
    expected[5] = 4
    if 0x29A768 in profiles and profiles[0x29A768][0] != expected:
        errors.append("NV descriptor 0x0749 did not yield the synthetic display field")
    if 0x2B1E80 in profiles and profiles[0x2B1E80][1][7] != 4:
        errors.append("firmware did not copy display-profile byte 5 to active slot 7")
    if 0x29AE68 in profiles:
        errors.append("display-profile update handler unexpectedly ran during coherent boot")
    return errors, {"boundaries": len(profiles), "update_seen": int(0x29AE68 in profiles)}


def check_lcd_io(text: str):
    errors = []
    selected = None
    commands = []
    data_bytes = 0
    writes_without_selection = 0
    for offset_text, data_text in IO_RE.findall(text):
        offset = int(offset_text, 16)
        data = int(data_text, 16)
        if offset == 0x2D:
            selected = data
        elif offset in (0x2E, 0x6E):
            if selected != 0x21:
                writes_without_selection += 1
                continue
            if offset == 0x6E:
                commands.append(data)
            else:
                data_bytes += 1
    if commands[:3] != [0x24, 0x40, 0x80]:
        errors.append(
            "LCD initialization prefix is not command bytes 24,40,80 after select 21"
        )
    if data_bytes < 504:
        errors.append(f"only {data_bytes} LCD data bytes observed, expected at least 504")
    if writes_without_selection:
        errors.append(f"{writes_without_selection} LCD writes occurred without select 0x21")
    return errors, {
        "commands": len(commands),
        "data_bytes": data_bytes,
        "unselected": writes_without_selection,
    }


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--firmware", choices=sorted(DESCRIPTORS), required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--eeprom", type=Path, required=True)
    parser.add_argument("--require-profile-boundary", action="store_true")
    args = parser.parse_args(argv)

    text = args.log.read_text(errors="replace")
    errors, descriptor = check_descriptor(
        args.rom.read_bytes(), args.eeprom.read_bytes(), args.firmware
    )
    io_errors, io = check_lcd_io(text)
    errors.extend(io_errors)
    if args.require_profile_boundary:
        profile_errors, profile = check_v600_profile(text)
        errors.extend(profile_errors)
    else:
        profile = {"boundaries": len(parse_profile(text)), "update_seen": 0}

    print(
        f"display NV: firmware={args.firmware} descriptor=0x0749 "
        f"offset=0x{descriptor['offset']:04x} length={descriptor['length']} "
        f"erased={int(descriptor['erased'])}"
    )
    print(
        f"display profile: boundaries={profile['boundaries']} "
        f"update_seen={profile['update_seen']}"
    )
    print(
        f"LCD transport: commands={io['commands']} data={io['data_bytes']} "
        f"unselected={io['unselected']}"
    )
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
