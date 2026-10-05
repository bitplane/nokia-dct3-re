#!/usr/bin/env python3
"""Verify physical SMS reading and preserved cold-boot content on NSM-2."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_incoming_sms_trace_check import (
    verify as verify_delivery, SMS_NVRAM_OFFSET, STORED_RECORD_PREFIX,
)


def verify(text: str, storage: bytes, preserved: bool = False) -> None:
    expected = b'\x01' + STORED_RECORD_PREFIX[1:]
    if storage[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(expected)] != expected:
        raise ValueError('missing exact read hello SMS in preserved SIM storage')
    if preserved:
        if 'sim_device: update fid=6f3c' in text or 'pd=09 message=01' in text:
            raise ValueError('cold readback rewrote or redelivered the message')
        if not re.search(r'ins=b2 p1=01 p2=04 p3=b0 selected=6f3c', text):
            raise ValueError('cold readback did not read the stored SIM SMS')
    else:
        verify_delivery(text, storage, 'nsm2', read=True)
        require_ordered(text, (
            ('handset CP-ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2')),
            ('handset RP-ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')),
            ('network CP-ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
            ('RR release', re.compile(r'LAPDm service Channel Release acknowledged')),
            ('return to paging', re.compile(r'PCH no-identity fill')),
        ), '8850 SMS transport closure')
    require_ordered(text, (
        ('physical Read', re.compile(r'8850_sms_physical: action=read_4')),
        ('Read decode', re.compile(r'8850_keypad_decoded key=19\b')),
    ), '8850 SMS UI')


def verify_frame(path: Path) -> None:
    with Image.open(path) as source:
        frame = source.convert('L')
    if frame.size != (84, 48):
        raise ValueError('wrong 8850 SMS frame geometry')
    # Message body only, excluding softkeys and any status indicators.
    digest = hashlib.sha256(frame.crop((0, 0, 84, 24)).tobytes()).hexdigest()
    if digest != '426de6fc34ebd2112536e8f3245696c996f624abf6d6569ead2c8c0651b49635':
        raise ValueError('8850 message body does not display reviewed hello pixels')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    parser.add_argument('frame', type=Path)
    parser.add_argument('--preserved', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), args.storage.read_bytes(), args.preserved)
        verify_frame(args.frame)
    except (ValueError, OSError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8850 SMS PASS: exact read storage, physical Read and reviewed hello frame')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
