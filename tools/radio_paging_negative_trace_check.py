#!/usr/bin/env python3
"""Verify that invalid or unmonitored pages do not reach RR access."""

import argparse
import pathlib
import re


EVIDENCE = {
    "wrong-group": re.compile(
        r"PCH off-group IMSI page not monitored channel=60 "
        r"air_fn=(\d+) monitor_fn=(\d+)"),
    "unmatched": re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
        r"31062110080910101032547608"),
    "malformed": re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
        r"31062110090910101032547698"),
}


def verify(text: str, profile: str) -> None:
    if profile not in EVIDENCE:
        raise ValueError(f"unknown paging-negative profile: {profile}")
    release = text.find("LAPDm Channel Release acknowledged nr=2")
    if release < 0:
        raise ValueError("missing completed registration before negative page")
    interval = text[release:]
    match = EVIDENCE[profile].search(interval)
    if not match:
        raise ValueError(f"missing {profile} page evidence")
    if profile == "wrong-group":
        air_frame, monitored_frame = map(int, match.groups())
        if air_frame == monitored_frame:
            raise ValueError("wrong-group page used the monitored DRX frame")

    after_page = interval[match.end():]
    forbidden = (
        ("CHANNEL REQUEST", re.compile(r"TX packet type=0c ")),
        ("Paging Response", re.compile(r"TX packet type=1b .*data=0080013f410627")),
        ("EF_LOCI mutation", re.compile(r"sim_device: update-binary fid=6f7e")),
    )
    for label, pattern in forbidden:
        if pattern.search(after_page):
            raise ValueError(f"negative page produced forbidden {label}")
    if not re.search(
            r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0",
            after_page):
        raise ValueError("idle PCH fill did not continue after rejected page")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--profile", required=True, choices=tuple(EVIDENCE))
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.profile)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - NHM-6 rejected the {args.profile} page without RR access")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
