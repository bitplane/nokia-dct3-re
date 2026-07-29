#!/usr/bin/env python3
"""Verify that invalid or unmonitored pages do not reach RR access."""

import argparse
import pathlib
import re

try:
    from tools.radio_mobile_identity import (
        PAGING_REQUEST_RE,
        registered_mobile_identity,
    )
except ModuleNotFoundError:
    from radio_mobile_identity import (
        PAGING_REQUEST_RE,
        registered_mobile_identity,
    )

EVIDENCE = {
    "wrong-group": re.compile(
        r"PCH off-group IMSI page not monitored channel=60 "
        r"air_fn=(\d+) monitor_fn=(\d+)"),
}
PROFILES = ("wrong-group", "unmatched", "malformed")


def verify(text: str, profile: str) -> None:
    if profile not in PROFILES:
        raise ValueError(f"unknown paging-negative profile: {profile}")
    identity = registered_mobile_identity(text)
    release = text.find("LAPDm Channel Release acknowledged nr=2")
    if release < 0:
        raise ValueError("missing completed registration before negative page")
    interval = text[release:]
    if profile == "wrong-group":
        match = EVIDENCE[profile].search(interval)
        if not match:
            raise ValueError("missing wrong-group page evidence")
        air_frame, monitored_frame = map(int, match.groups())
        if air_frame == monitored_frame:
            raise ValueError("wrong-group page used the monitored DRX frame")
    else:
        match = PAGING_REQUEST_RE.search(interval)
        if not match:
            raise ValueError(f"missing {profile} page evidence")
        if profile == "unmatched":
            if match.group("length") != "08":
                raise ValueError("unmatched page is not a valid identity LV")
            if match.group("identity") == identity:
                raise ValueError("unmatched page addressed registered subscriber")
        else:
            if match.group("length") == "08":
                raise ValueError("malformed page has a valid identity LV length")
            if match.group("identity") != identity:
                raise ValueError("malformed page changed the registered identity")

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
    parser.add_argument("--profile", required=True, choices=PROFILES)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors="replace"), args.profile)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(f"OK - handset rejected the {args.profile} page without RR access")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
