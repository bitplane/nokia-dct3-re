#!/usr/bin/env python3
"""Verify that NHM-6 organically rejects standards-shaped unsuitable cells."""

import argparse
import pathlib
import re


EVIDENCE = {
    "barred": "49061b000100f110000140000000000002",
    "rxlev": "49061b000100f110000140000000003f",
}

FORBIDDEN = (
    ("automatic-access publication",
     re.compile(r"TX packet type=46 .*data=3210321000010000")),
    ("CHANNEL REQUEST", re.compile(r"TX packet type=0c ")),
    ("Location Updating Request",
     re.compile(r"TX packet type=1b .*data=.*490508")),
    ("EF_LOCI write",
     re.compile(r"sim_device: update-binary fid=6f7e")),
)


def verify(text: str, profile: str) -> None:
    text = text.replace("[:] ", "")
    if profile not in EVIDENCE:
        raise ValueError(f"unknown unsuitable-cell profile: {profile}")
    decoded_si3 = tuple(
        re.sub(r"[^0-9a-f]", "", line.split("data=", 1)[1].split("task=", 1)[0])
        for line in text.splitlines()
        if "nhm6_bcch_parse: channel=50" in line and "data=" in line)
    if not any(EVIDENCE[profile] in block for block in decoded_si3):
        raise ValueError(
            f"missing decoded NHM-6 {profile} cell-suitability evidence")
    if not re.search(r"TX packet type=57 .*data=03050000", text):
        raise ValueError("firmware did not remain on the incomplete-cell path")
    for label, pattern in FORBIDDEN:
        if pattern.search(text):
            raise ValueError(f"unsuitable cell reached forbidden {label}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--profile", required=True, choices=tuple(EVIDENCE))
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.profile)
    except ValueError as error:
        print(error)
        return 1
    print(f"OK - NHM-6 organically rejected the {args.profile} cell")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
