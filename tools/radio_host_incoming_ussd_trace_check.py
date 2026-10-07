#!/usr/bin/env python3
"""Verify host-originated USSD notification delivery."""

import pathlib
import re
import argparse
from hashlib import sha256


BASE_CHECKPOINTS = (
    re.compile(r"gsm_call_adapter: network registered=1 arfcn=1"),
    re.compile(r"gsm_call_adapter: incoming ussd id=1 result=accepted"),
    re.compile(r"gsm_call_adapter: incoming ussd state id=1 .*phase=queued"),
)
SERVICE_CHECKPOINTS = (
    re.compile(r"gsm_ss: network_initiated operation=notify"),
    re.compile(r"LAPDm service Channel Release acknowledged"),
    re.compile(r"gsm_call_adapter: incoming ussd state id=1 .*phase=delivered"),
)

SEVEN_TEXT_FRAME = '20b41005eba4d1143df1299ac66a05bc4b081bef62a3b0d2e378c9b1879ec4ab'


def verify_seven_text_frame(pixels: bytes, size: tuple[int, int]) -> None:
    if size != (84, 48) or sha256(pixels).hexdigest() != SEVEN_TEXT_FRAME:
        raise ValueError('missing reviewed 1234567 USSD frame without trailing @')


def verify(text: str, require_restore: bool = False) -> None:
    checkpoints = list(BASE_CHECKPOINTS)
    if require_restore:
        checkpoints.extend((
            re.compile(r"state_roundtrip: result=pass"),
            re.compile(r"gsm_call_adapter: incoming ussd state id=1 epoch=2 phase=queued"),
        ))
    checkpoints.extend(SERVICE_CHECKPOINTS)
    cursor = 0
    for pattern in checkpoints:
        match = pattern.search(text, cursor)
        if not match:
            raise ValueError(f"missing incoming host-USSD checkpoint: {pattern.pattern}")
        cursor = match.end()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=pathlib.Path)
    parser.add_argument("--require-restore", action="store_true")
    parser.add_argument('--seven-text-frame', type=pathlib.Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(), args.require_restore)
        if args.seven_text_frame:
            from PIL import Image
            with Image.open(args.seven_text_frame) as image:
                verify_seven_text_frame(image.convert('L').tobytes(), image.size)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    print("OK - host USSD notification completed the firmware/RR lifecycle")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
