"""Check NSM-3 incoming SMS closure, physical reading and persisted content."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_incoming_sms_trace_check import (
    COMMON_CHECKPOINTS, SMS_NVRAM_OFFSET, STORED_RECORD_PREFIX,
)


def verify(text, storage):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    checkpoints = COMMON_CHECKPOINTS[:5] + ((
        'own DSP cipher control', re.compile(
            r'TX packet type=14 payload=12 .*data=0080ffffffffffffffff0000'),
    ),) + COMMON_CHECKPOINTS[5:] + (
        ('CP ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2')),
        ('RP ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')),
        ('network CP ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('physical read', re.compile(r'8210_sms_physical: action=read_2')),
        ('read key', re.compile(r'8210_keypad_decoded: key=19\b')),
    )
    require_ordered(text, checkpoints, '8210 incoming SMS')
    if text.count('PCH IMSI page transmitted channel=60') != 1:
        raise ValueError('expected exactly one SMS page')
    if text.count('sim_device: update fid=6f3c record=1 length=176') != 2:
        raise ValueError('expected delivery and read-status writes only')
    expected = b'\x01' + STORED_RECORD_PREFIX[1:]
    if storage[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(expected)] != expected:
        raise ValueError('missing persistent read hello SMS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'dsp_hle:' in line or
                           'RX enqueue' in line or 'sim_device:' in line or
                           '8210_sms_physical' in line or '8210_keypad_decoded' in line or
                           '[LUA ERROR]' in line)
        verify(text, args.storage.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 incoming SMS FAIL: {error}\n')
    print('8210 incoming SMS transport, physical read and persistent hello PASS; inspect UI separately')
