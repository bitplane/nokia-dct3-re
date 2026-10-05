"""Verify NSB-6 paging, physical Answer/End and complete call release."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import (
    REGISTRATION_RELEASE, IMSI_PAGE, PAGING_RESPONSE, PAGING_CONTENTION_UA,
    CIPHER_MODE_COMMAND, CIPHER_MODE_COMPLETE, MM_INFORMATION, INCOMING_SETUP,
    ALERTING, TRAFFIC_SABM, TRAFFIC_UA, ASSIGNMENT_COMPLETE, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, NETWORK_RELEASE, RELEASE_COMPLETE,
    RR_CHANNEL_RELEASE, TRAFFIC_RELEASE_UA, RELEASE_CONFIRMATION, IDLE_PCH,
    require_count, require_ordered,
)
from tools.noki8890_registration_check import verify as verify_registration

CHECKPOINTS = (
    ('registration release', REGISTRATION_RELEASE), ('IMSI page', IMSI_PAGE),
    ('Paging Response', PAGING_RESPONSE), ('contention UA', PAGING_CONTENTION_UA),
    ('Cipher Mode Command', CIPHER_MODE_COMMAND), ('MM Information', MM_INFORMATION),
    ('Cipher Mode Complete', CIPHER_MODE_COMPLETE), ('incoming SETUP', INCOMING_SETUP),
    ('Call Confirmed', re.compile(r'GSM service uplink sapi=0 pd=03 message=08 length=11')),
    ('Alerting', ALERTING),
    ('own traffic configuration', re.compile(r'TX packet type=02 payload=20 .*data=040002000271012fc10000010000000400000000')),
    ('traffic SABM', TRAFFIC_SABM), ('traffic UA', TRAFFIC_UA),
    ('Assignment Complete', ASSIGNMENT_COMPLETE),
    ('physical Answer', re.compile(r'8890_incoming_physical: action=Call / Send')),
    ('Answer decode', re.compile(r'8890_keypad_decoded: key=0e\b')),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'8890_incoming_physical: action=End')),
    ('End decode', re.compile(r'8890_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('network Release', NETWORK_RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_CHANNEL_RELEASE),
    ('traffic release UA', TRAFFIC_RELEASE_UA),
    ('own release configuration', re.compile(r'TX packet type=02 payload=20 .*data=040000001117001a6000003c0000001400000001')),
    ('release confirmation', RELEASE_CONFIRMATION), ('idle PCH', IDLE_PCH),
)


def verify(text, *, pcs1900=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    checkpoints = CHECKPOINTS
    if pcs1900:
        verify_registration(text, pcs1900=True)
        checkpoints = tuple((label, re.compile(
            r'TX packet type=02 payload=20 .*data=041202000271012fc10002580000000400000000'
            if label == 'own traffic configuration' else
            r'TX packet type=02 payload=20 .*data=041202001117001a600002580000001400000001'
            if label == 'own release configuration' else pattern.pattern))
            for label, pattern in CHECKPOINTS)
    require_ordered(text, checkpoints, '8890 incoming signaling')
    for label, pattern in (('incoming SETUP', INCOMING_SETUP), ('Connect', CONNECT), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'exactly one {label}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--pcs1900', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), pcs1900=args.pcs1900)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 incoming FAIL: {error}\n')
    print('8890 incoming physical Answer/End and CC/RR signaling PASS; speech unproved')
