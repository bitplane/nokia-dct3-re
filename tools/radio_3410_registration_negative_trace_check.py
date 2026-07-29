#!/usr/bin/env python3
"""Verify NHM-2 rejects unsuitable cells and unmatched assignments."""

import argparse
import pathlib
import re


SI3_EVIDENCE = {
    "barred": "49061b000100f110000140000000000002",
    "rxlev": "49061b000100f110000140000000003f",
}


def _forbid_registration(text: str) -> None:
    forbidden = (
        ("Location Updating Request",
         r"TX packet type=1b .*data=.*490508"),
        ("EF_LOCI mutation", r"sim_device: update-binary fid=6f7e"),
    )
    for label, pattern in forbidden:
        if re.search(pattern, text):
            raise ValueError(f"negative NHM-2 gate reached forbidden {label}")


def verify(text: str, profile: str) -> None:
    text = text.replace("[:] ", "")
    if profile in SI3_EVIDENCE:
        if SI3_EVIDENCE[profile] not in text:
            raise ValueError(f"missing delivered {profile} SI3 evidence")
        if "TX packet type=0c " in text:
            raise ValueError("unsuitable cell produced CHANNEL REQUEST")
        _forbid_registration(text)
        return

    if profile != "assignment":
        raise ValueError(f"unknown NHM-2 negative profile: {profile}")
    request = re.search(
        r"TX packet type=0c .*data=0000([0-9a-f]{2})[0-9a-f]*", text)
    if not request:
        raise ValueError("missing organic CHANNEL REQUEST")
    assignments = re.findall(
        r"RX enqueue type=80 payload=34 .*data=([0-9a-f]+)", text)
    assignment = next(
        (data[data.index("2d063f"):] for data in assignments
         if "2d063f" in data), None)
    if assignment is None or len(assignment) < 16:
        raise ValueError("missing Immediate Assignment")
    echoed_ra = assignment[14:16]
    if echoed_ra == request.group(1):
        raise ValueError("Immediate Assignment unexpectedly matched request")
    if re.search(
            r"TX packet type=02 .*radio_phase=random_access "
            r".*data=[0-9a-f]{16}80", text):
        raise ValueError("mismatched assignment configured the SDCCH")
    _forbid_registration(text)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument(
        "--profile", required=True,
        choices=("barred", "rxlev", "assignment"))
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.profile)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - NHM-2 organically rejected {args.profile}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
