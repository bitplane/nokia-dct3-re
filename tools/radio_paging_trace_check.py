#!/usr/bin/env python3
"""Verify one organic network-originated paging lifecycle."""

import pathlib
import re
import sys

try:
    from tools.radio_mobile_identity import (
        PAGING_REQUEST_RE,
        PAGING_RESPONSE_RE,
        paging_group,
        registered_mobile_identity,
    )
except ModuleNotFoundError:
    from radio_mobile_identity import (
        PAGING_REQUEST_RE,
        PAGING_RESPONSE_RE,
        paging_group,
        registered_mobile_identity,
    )

CHECKPOINTS = (
    ("registration release", re.compile(
        r"LAPDm Channel Release acknowledged nr=2")),
    ("no-identity PCH fill", re.compile(
        r"PCH no-identity fill channel=60 fn=")),
    ("IMSI Paging Request Type 1", PAGING_REQUEST_RE),
    ("page publication", re.compile(
        r"PCH IMSI page transmitted channel=60 fn=")),
    ("paged random access", re.compile(
        r"TX packet type=0c .*data=00[0-9a-f]{4}")),
    ("correlated Immediate Assignment", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}2d063f")),
    ("Paging Response", PAGING_RESPONSE_RE),
    ("contention-resolution UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*0173410627")),
    ("paging transaction release", re.compile(
        r"LAPDm Channel Release acknowledged nr=1")),
    ("return to PCH fill", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0")),
)


def verify(text: str) -> None:
    identity = registered_mobile_identity(text)
    page = PAGING_REQUEST_RE.search(text)
    if not page or page.group("length") != "08":
        raise ValueError("missing or malformed IMSI Paging Request Type 1")
    if page.group("identity") != identity:
        raise ValueError("IMSI Paging Request does not address registered subscriber")
    response = PAGING_RESPONSE_RE.search(text)
    if not response:
        raise ValueError("missing Paging Response")
    if response.group("identity") != identity:
        raise ValueError("Paging Response does not carry registered subscriber")

    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order paging checkpoint: {label}")
        cursor = match.end()

    pages = re.findall(r"PCH IMSI page transmitted channel=60", text)
    if len(pages) != 1:
        raise ValueError(f"expected exactly one IMSI page, observed {len(pages)}")

    # Candidate acquisition can expose PCH before Location Updating. Validate
    # only the post-registration interval against the organically registered
    # IMSI's standards-derived group.
    registration_release = text.find("LAPDm Channel Release acknowledged nr=2")
    post_registration = text[registration_release:]
    paging_frames = [
        int(value) for value in re.findall(
            r"PCH (?:no-identity fill|IMSI page transmitted) channel=60 fn=(\d+)",
            post_registration)
    ]
    phase, offset = paging_group(identity)
    if not paging_frames or any(
            frame % 51 != offset or (frame // 51) % 2 != phase
            for frame in paging_frames):
        raise ValueError(
            f"PCH blocks missed registered IMSI paging group: {paging_frames}")


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: radio_paging_trace_check.py MAME_ERROR_LOG")
    try:
        verify(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - PCH fill, IMSI page, Paging Response and release completed organically")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
