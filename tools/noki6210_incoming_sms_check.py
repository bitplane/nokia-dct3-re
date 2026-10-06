"""NPE-3 SMS transport closure, physical reading and persistent content."""
import re
from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_incoming_sms_trace_check import (
    COMMON_CHECKPOINTS, SMS_NVRAM_OFFSET, STORED_RECORD_PREFIX,
)


def verify(text, storage):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    checkpoints = COMMON_CHECKPOINTS + (
        ('CP ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=04 length=2')),
        ('RP ACK', re.compile(r'GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240')),
        ('network CP ACK', re.compile(r'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2')),
        ('release', re.compile(r'LAPDm service Channel Release acknowledged')),
        ('physical read', re.compile(r'6210_sms_physical: action=read_2')),
        ('read key', re.compile(r'6210_keypad_decoded: key=19\b')),
    )
    require_ordered(text, checkpoints, '6210 incoming SMS')
    if text.count('PCH IMSI page transmitted channel=60') != 1:
        raise ValueError('expected exactly one SMS page')
    if text.count('sim_device: update fid=6f3c record=1 length=176') != 2:
        raise ValueError('expected delivery and read-status writes only')
    expected = b'\x01' + STORED_RECORD_PREFIX[1:]
    if storage[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET + len(expected)] != expected:
        raise ValueError('missing persistent read hello SMS')
