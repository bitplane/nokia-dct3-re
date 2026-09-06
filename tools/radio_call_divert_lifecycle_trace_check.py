#!/usr/bin/env python3
"""Verify one coherent unconditional-forwarding lifecycle."""

import argparse
import hashlib
import pathlib
import re


EVENTS = (
    re.compile(r"gsm_ss: request=register transaction=1b invoke=1 service=21 "
               r"number_length=5 active=1\s"),
    re.compile(r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=33\s"),
    re.compile(r"gsm_ss: request=interrogate transaction=1b invoke=2 service=21 active=1\s"),
    re.compile(r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=21\s"),
    re.compile(r"gsm_ss: request=deactivate transaction=1b invoke=3 service=21 active=0\s"),
    re.compile(r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=33\s"),
    re.compile(r"gsm_ss: request=interrogate transaction=1b invoke=4 service=21 active=0\s"),
    re.compile(r"GSM service downlink kind=27 sapi=0 pd=0b message=2a length=17\s"),
)
ACTIVE_NUMBER_FRAME = "dbf6380cfd05b619dcc3fd5d2d7e918903c8afdc4c9b8b4f873829c7e843e670"
INACTIVE_FRAME = "e1f8fa4d2791e6a1c0c7cf7afe78d4e64be7d9c7c689a4539c893a420a9df224"


def verify(text: str, frame_dir: pathlib.Path, require_frames: bool = True) -> dict:
    cursor = 0
    for index, pattern in enumerate(EVENTS, 1):
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing or misordered lifecycle event {index}")
        cursor = match.end()
    releases = list(re.finditer(r"radio_phase=release_channel_change", text))
    if len(releases) < 5:  # Initial LU plus four supplementary transactions.
        raise ValueError("fewer than four supplementary RR releases completed")
    if not re.search(r"state_roundtrip: result=pass\b", text):
        raise ValueError("missing successful save-state round trip")
    hashes = {
        hashlib.sha256(path.read_bytes()).hexdigest()
        for path in frame_dir.glob("*.pgm")
    }
    if require_frames and ACTIVE_NUMBER_FRAME not in hashes:
        raise ValueError("missing active forwarding-number frame")
    if require_frames and INACTIVE_FRAME not in hashes:
        raise ValueError("missing final inactive-service frame")
    return {"events": len(EVENTS), "releases": len(releases), "frames": len(hashes)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("frame_dir", type=pathlib.Path)
    args = parser.parse_args()
    try:
        result = verify(args.log.read_text(errors="replace"), args.frame_dir)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - organic call-forward register/query/deactivate/query lifecycle "
          f"completed ({result['releases']} RR releases)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
