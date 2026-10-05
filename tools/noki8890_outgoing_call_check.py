"""Check NSB-6 outgoing signaling and explicit called-number expectation."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_count, require_ordered
from tools.noki8890_registration_check import verify as verify_registration
from tools.radio_outgoing_call_trace_check import (
    CM_SERVICE_REQUEST, CM_SERVICE_ACCEPT, SETUP, CALL_PROCEEDING,
    TRAFFIC_ASSIGNMENT, ASSIGNMENT_COMPLETE, ALERTING, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, RELEASE, RELEASE_COMPLETE, RR_RELEASE,
    PCH, decode_called_digits,
)

CHECKPOINTS = (
    ('physical Send', re.compile(r'8890_call_physical: action=send')),
    ('Send decode', re.compile(r'8890_keypad_decoded: key=0e\b')),
    ('CM Service Request', CM_SERVICE_REQUEST), ('CM Service Accept', CM_SERVICE_ACCEPT),
    ('SETUP', SETUP), ('Call Proceeding', CALL_PROCEEDING),
    ('assignment', TRAFFIC_ASSIGNMENT),
    ('own traffic configuration', re.compile(r'TX packet type=02 payload=20 .*data=040002000271012fc10000010000000400000000')),
    ('Assignment Complete', ASSIGNMENT_COMPLETE), ('Alerting', ALERTING),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'8890_call_physical: action=end')),
    ('End decode', re.compile(r'8890_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('Release', RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_RELEASE),
    ('own release configuration', re.compile(r'TX packet type=02 payload=20 .*data=040000001117001a6000003c0000001400000001')),
    ('idle confirmation', re.compile(r'RX enqueue type=89 payload=8 .*data=0000000000000000')),
    ('return to paging', PCH),
)


def verify(text, number='1234567', *, pcs1900=False):
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
    require_ordered(text, checkpoints, '8890 outgoing signaling')
    for label, pattern in (('SETUP', SETUP), ('assignment', TRAFFIC_ASSIGNMENT),
                           ('Connect Acknowledge', CONNECT_ACKNOWLEDGE), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'8890 exactly one {label}')
    setup = SETUP.search(text)
    data = bytes.fromhex(setup.group('data'))
    if len(data) != int(setup.group('length')) or decode_called_digits(data) != number:
        raise ValueError('SETUP number differs from explicitly expected physical number')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--number', default='1234567')
    parser.add_argument('--pcs1900', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), args.number, pcs1900=args.pcs1900)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 outgoing FAIL: {error}\n')
    print(f'8890 outgoing signaling PASS for {args.number}; speech unproved')
