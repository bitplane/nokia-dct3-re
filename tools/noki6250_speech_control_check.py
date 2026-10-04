#!/usr/bin/env python3
"""Verify NHM-3's own speech-control field, not a working PCM/audio path."""
import argparse
from pathlib import Path
import re


def recover(image):
    def u16(address):
        offset = address - 0x200000
        if offset < 0 or offset + 2 > len(image):
            raise ValueError("ROM table address outside image")
        return int.from_bytes(image[offset:offset + 2], "big")

    def u32(address):
        return (u16(address) << 16) | u16(address + 2)

    if u32(0x429aa8) != 0x429de6:
        raise ValueError("command-8 decoder table differs")
    if u32(0x42a138) != 0xffff8000 or u32(0x42a108) != 0x100a8:
        raise ValueError("command-8 wire encoding/destination differs")
    if u32(0x3fbfec) != 0x263160 or u32(0x3fc028) != 0x263154:
        raise ValueError("compiler field-table pointers differ")
    adds = [u16(0x263154 + 2*index) for index in range(5)]
    keep = u16(0x263160)
    if adds != [0x200]*5 or keep != 0xfdff:
        raise ValueError("speech add/remove field differs")
    return 0x200


def verify(image, log):
    field = recover(image)
    cursor = 0
    patterns = (
        r"6250_audio_field: .*selector=01 keep=fdff add=0200 before=0426",
        r"6250_audio_helper: command=08 value=0626 commit=1 caller=003fc33f",
        r"doorbell .*wire=8626 speech_control=0626",
        r"6250_audio_helper: command=08 value=0426 commit=1 caller=003fc33f",
        r"doorbell .*wire=8426 speech_control=0426",
    )
    for pattern in patterns:
        match = re.compile(pattern).search(log, cursor)
        if not match:
            raise ValueError(f"missing/out-of-order control evidence: {pattern}")
        cursor = match.end()
    return field


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", type=Path)
    parser.add_argument("log", type=Path)
    args = parser.parse_args()
    try:
        field = verify(args.rom.read_bytes(), args.log.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(f"PASS: NHM-3 own speech-control field {field:04x}; PCM/audio not established")


if __name__ == "__main__":
    main()
