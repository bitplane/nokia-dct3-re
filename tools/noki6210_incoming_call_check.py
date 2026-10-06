"""Own NPE-3 incoming paging and physical Answer/End signaling."""
import re
from tools.radio_call_lifecycle_common import (
    REGISTRATION_RELEASE, IMSI_PAGE, PAGING_RESPONSE, PAGING_CONTENTION_UA,
    CIPHER_MODE_COMMAND, CIPHER_MODE_COMPLETE, MM_INFORMATION, INCOMING_SETUP,
    ALERTING, TRAFFIC_SABM, TRAFFIC_UA, ASSIGNMENT_COMPLETE, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, NETWORK_RELEASE, RELEASE_COMPLETE,
    RR_CHANNEL_RELEASE, TRAFFIC_RELEASE_UA, RELEASE_CONFIRMATION, IDLE_PCH,
    require_count, require_ordered,
)

CHECKPOINTS = (
    ('registration release', REGISTRATION_RELEASE), ('IMSI page', IMSI_PAGE),
    ('Paging Response', PAGING_RESPONSE), ('contention UA', PAGING_CONTENTION_UA),
    ('Cipher Mode Command', CIPHER_MODE_COMMAND), ('MM Information', MM_INFORMATION),
    ('Cipher Mode Complete', CIPHER_MODE_COMPLETE), ('incoming SETUP', INCOMING_SETUP),
    ('own Call Confirmed', re.compile(r'GSM service uplink sapi=0 pd=03 message=08 length=11 data=8308040460020081150101')),
    ('Alerting', ALERTING),
    ('own traffic configuration', re.compile(r'TX packet type=02 payload=24 .*data=040002000271012fc1000001000000040000000000000000')),
    ('traffic SABM', TRAFFIC_SABM), ('traffic UA', TRAFFIC_UA),
    ('Assignment Complete', ASSIGNMENT_COMPLETE),
    ('physical Answer', re.compile(r'6210_incoming_physical: action=Send')),
    ('Answer decode', re.compile(r'6210_keypad_decoded: key=0e\b')),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'6210_incoming_physical: action=End')),
    ('End decode', re.compile(r'6210_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('network Release', NETWORK_RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_CHANNEL_RELEASE),
    ('traffic release UA', TRAFFIC_RELEASE_UA),
    ('own release configuration', re.compile(r'TX packet type=02 payload=24 .*data=040000001117001a60000023000000140000000100000000')),
    ('release confirmation', RELEASE_CONFIRMATION), ('idle PCH', IDLE_PCH),
)


def verify(text):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    require_ordered(text, CHECKPOINTS, '6210 incoming signaling')
    for label, pattern in (('incoming SETUP', INCOMING_SETUP), ('Connect', CONNECT), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'6210 exactly one {label}')
