#!/usr/bin/env python3
"""Verify the paired NSE-8 command-0x08 speech/radio bit-field tables."""

import argparse
from dataclasses import dataclass
from pathlib import Path


FLASH_BASE = 0x200000


@dataclass(frozen=True)
class Profile:
    name: str
    add_speech: int
    remove_speech: int
    add_channel: int
    remove_channel: int


PROFILES = (
    Profile("v600", 0x2DA1A0, 0x2DA1A8, 0x2DA1AC, 0x2DA1B4),
    Profile("v501", 0x2CFC04, 0x2CFC0C, 0x2CFC10, 0x2CFC18),
)


def logical_halfword(image: bytes, address: int) -> int:
    offset = address - FLASH_BASE
    if offset < 0 or offset + 2 > len(image):
        raise ValueError(f"address 0x{address:08x} is outside image")
    # The static input is the repository's swap16 representation.
    return int.from_bytes(image[offset:offset + 2], "little")


def verify_profile(image: bytes, profile: Profile) -> dict[str, int]:
    values = {
        "speech_add": logical_halfword(image, profile.add_speech),
        "speech_keep_mask": logical_halfword(image, profile.remove_speech),
        "channel_add": logical_halfword(image, profile.add_channel),
        "channel_keep_mask": logical_halfword(image, profile.remove_channel),
    }
    expected = {
        "speech_add": 0x0201,
        "speech_keep_mask": 0xFDFE,
        "channel_add": 0x0408,
        "channel_keep_mask": 0xFBF3,
    }
    if values != expected:
        rendered = ", ".join(f"{key}={value:04x}" for key, value in values.items())
        raise ValueError(f"{profile.name}: command-0x08 table mismatch ({rendered})")
    if values["speech_add"] & values["speech_keep_mask"]:
        raise ValueError(f"{profile.name}: speech add/remove fields disagree")
    if values["channel_add"] & values["channel_keep_mask"]:
        raise ValueError(f"{profile.name}: channel add/remove fields disagree")
    return values


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--v600", type=Path, required=True)
    parser.add_argument("--v501", type=Path, required=True)
    args = parser.parse_args()

    paths = {"v600": args.v600, "v501": args.v501}
    results = {
        profile.name: verify_profile(paths[profile.name].read_bytes(), profile)
        for profile in PROFILES
    }
    if results["v600"] != results["v501"]:
        raise SystemExit("FAIL - paired ROM command-0x08 tables differ")
    print(
        "OK - paired ROMs encode independent command-0x08 fields "
        "speech=0201 and dedicated-channel=0408"
    )


if __name__ == "__main__":
    main()
