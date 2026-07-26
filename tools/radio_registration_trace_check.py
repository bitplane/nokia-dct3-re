#!/usr/bin/env python3
"""Verify one organic GSM registration and return-to-idle lifecycle."""

import argparse
import pathlib
import re


COMMON_CHECKPOINTS_BEFORE_ACCEPT = (
    ("random access", re.compile(r"TX packet type=0c .*data=000[0-9a-f]{3}")),
    ("Location Updating Request", re.compile(
        r"TX packet type=1b .*data=0080013f[0-9a-f]*490508")),
    ("contention-resolution UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*"
        r"01734905087000f000fffe330809101010325476982b2b2b")),
)

COMMON_CHECKPOINTS_AFTER_ACCEPT = (
    ("dedicated uplink request after Location Updating Accept", re.compile(
        r"RX enqueue type=86 payload=8 .*data=8000000000000000")),
    ("Location Updating Accept LAPDm acknowledgement", re.compile(
        r"LAPDm Location Updating Accept acknowledged nr=1")),
    ("handset SAPI-0 RR N(R)=1", re.compile(
        r"TX packet type=1b .*data=0080032101")),
    ("dedicated uplink request after RR Channel Release", re.compile(
        r"RX enqueue type=86 payload=8 .*data=8000000000000000")),
    ("RR Channel Release LAPDm acknowledgement", re.compile(
        r"LAPDm Channel Release acknowledged nr=2")),
    ("handset SAPI-0 RR N(R)=2", re.compile(
        r"TX packet type=1b .*data=0080034101")),
    ("EF_LOCI location update", re.compile(
        r"sim_device: update-binary fid=6f7e offset=4 length=5")),
    ("EF_LOCI status update", re.compile(
        r"sim_device: update-binary fid=6f7e offset=10 length=1")),
)

PROFILE_CHECKPOINTS = {
    "nse8": (
        ("Location Updating Accept", re.compile(
            r"radio_mm_parse: phase=return .*result=00000048 ")),
        ("operator presentation", re.compile(
            r"operator-resource data=.*00.*f1.*10.*01")),
    ),
    "nhm5": (
        ("Location Updating Accept", re.compile(
            r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}"
            r"030045050200f11000011708")),
    ),
}

PROFILE_ARFCN = {
    "nse8": "0001",
    "nhm5": "0058",
}

COMMON_POST_ACCEPT_CHECKPOINTS = (
    ("idle-channel confirmation", re.compile(r"radio peer RX type=89 .*")),
    ("no-identity PCH fill", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0")),
)

def verify(text: str, profile: str = "nse8") -> None:
    if profile not in PROFILE_CHECKPOINTS:
        raise ValueError(f"unknown registration profile: {profile}")

    checkpoints = (
        COMMON_CHECKPOINTS_BEFORE_ACCEPT
        + PROFILE_CHECKPOINTS[profile]
        + COMMON_CHECKPOINTS_AFTER_ACCEPT
        + (("RR channel deconfiguration", re.compile(
            r"TX packet type=02 .*radio_phase=release_channel_change "
            rf"data=041202000000001a6000{PROFILE_ARFCN[profile]}"
            r"0000000f00000000")),)
        + COMMON_POST_ACCEPT_CHECKPOINTS
    )
    cursor = 0
    for label, pattern in checkpoints:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order registration checkpoint: {label}")
        cursor = match.end()

    requests = re.findall(r"TX packet type=1b .*data=0080013f[0-9a-f]*490508", text)
    if len(requests) != 1:
        raise ValueError(f"expected one Location Updating Request, observed {len(requests)}")

    if profile == "nhm5":
        assigned_confirmation = re.search(
            r"radio_phase=assigned_channel_change[^\n]*"
            r"(?:\n.*)*?RX enqueue type=89 payload=8 .*data=0100000000000000",
            text,
        )
        if not assigned_confirmation:
            raise ValueError(
                "missing NHM-5 assigned-channel confirmation value one")
        release_confirmation = re.search(
            r"radio_phase=release_channel_change[^\n]*"
            r"(?:\n.*)*?RX enqueue type=89 payload=8 .*data=0000000000000000",
            text,
        )
        if not release_confirmation:
            raise ValueError(
                "missing NHM-5 release-channel confirmation value zero")

    release = text.find("radio_phase=release_channel_change")
    steady_bcch = len(re.findall(
        r"RX enqueue type=80 payload=34 .*data=50", text[release:]))
    if steady_bcch < 4:
        raise ValueError(
            f"serving BCCH did not remain active after release: {steady_bcch} blocks")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="verify one organic registration and return-to-idle lifecycle")
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--profile", choices=tuple(PROFILE_CHECKPOINTS), default="nse8")
    args = parser.parse_args()
    try:
        verify(args.log.read_text(), args.profile)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print(
        f"OK - {args.profile} Location Updating accepted, SIM persisted, "
        "channel released, and steady camp resumed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
