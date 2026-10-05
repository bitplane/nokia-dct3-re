#!/usr/bin/env python3
"""Check NSB-6 network SMS delivery, physical reading and persistent content."""
import argparse
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.radio_call_lifecycle_common import require_ordered
from tools.noki8890_registration_check import verify as verify_registration
from tools.radio_incoming_sms_trace_check import (
    COMMON_CHECKPOINTS, SMS_NVRAM_OFFSET, STORED_RECORD_PREFIX,
)


def verify(text, storage, *, pcs1900=False):
    if pcs1900:
        verify_registration(text, pcs1900=True)
    checkpoints = COMMON_CHECKPOINTS[:5] + ((
        'own DSP cipher control', re.compile(
            r'TX packet type=14 payload=12 .*data=0076ffffffffffffffff0000'),
    ),) + COMMON_CHECKPOINTS[5:] + (
        ('CP ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2')),
        ('RP ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')),
        ('network CP ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('physical read', re.compile(r'8890_sms_physical: action=read_2')),
        ('read key', re.compile(r'8890_keypad_decoded: key=19\b')),
    )
    require_ordered(text, checkpoints, '8890 incoming SMS')
    if text.count('PCH IMSI page transmitted channel=60') != 1:
        raise ValueError('expected exactly one incoming SMS page')
    if text.count('sim_device: update fid=6f3c record=1 length=176') != 2:
        raise ValueError('expected only delivery and read-status writes')
    expected = b'\x01' + STORED_RECORD_PREFIX[1:]
    if storage[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(expected)] != expected:
        raise ValueError('missing persistent read hello SMS')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    parser.add_argument('frame', type=Path)
    parser.add_argument('--pcs1900', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), args.storage.read_bytes(), pcs1900=args.pcs1900)
        import hashlib
        with Image.open(args.frame) as source:
            frame = source.convert('L')
        if frame.size != (84, 48) or hashlib.sha256(
                frame.crop((0, 0, 84, 24)).tobytes()).hexdigest() != (
                '426de6fc34ebd2112536e8f3245696c996f624abf6d6569ead2c8c0651b49635'):
            raise ValueError('missing reviewed hello body pixels')
    except (ValueError, OSError) as error:
        print(f'FAIL - {error}', file=sys.stderr)
        return 1
    print('8890 incoming SMS PASS: transport closure, physical read, persistent hello')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
