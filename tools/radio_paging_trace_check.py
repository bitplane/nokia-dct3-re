#!/usr/bin/env python3
"""Verify one organic network-originated paging lifecycle."""

import pathlib
import re
import sys


CHECKPOINTS = (
    ("registration release", re.compile(
        r"LAPDm Channel Release acknowledged nr=2")),
    ("no-identity PCH fill", re.compile(
        r"PCH no-identity fill channel=60 fn=")),
    ("IMSI Paging Request Type 1", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}"
        r"31062110080910101032547698")),
    ("page publication", re.compile(
        r"PCH IMSI page transmitted channel=60 fn=")),
    ("paged random access", re.compile(
        r"TX packet type=0c .*data=0000[0-9a-f]{2}")),
    ("correlated Immediate Assignment", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}2d063f")),
    ("Paging Response", re.compile(
        r"TX packet type=1b .*data=0080013f410627")),
    ("contention-resolution UA", re.compile(
        r"RX enqueue type=80 payload=34 .*data=80[0-9a-f]*0173410627")),
    ("paging transaction release", re.compile(
        r"LAPDm Channel Release acknowledged nr=1")),
    ("return to PCH fill", re.compile(
        r"RX enqueue type=80 payload=34 .*data=60[0-9a-f]{18}1506210001f0")),
)


def verify(text: str) -> None:
    cursor = 0
    for label, pattern in CHECKPOINTS:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or out-of-order paging checkpoint: {label}")
        cursor = match.end()

    pages = re.findall(r"PCH IMSI page transmitted channel=60", text)
    if len(pages) != 1:
        raise ValueError(f"expected exactly one IMSI page, observed {len(pages)}")

    paging_frames = [
        int(value) for value in re.findall(
            r"PCH (?:no-identity fill|IMSI page transmitted) channel=60 fn=(\d+)",
            text)
    ]
    if not paging_frames or any(
            frame % 51 != 36 or (frame // 51) % 2 != 1
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
