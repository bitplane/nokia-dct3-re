"""Check own NSM-3 physical outgoing CC/RR lifecycle; speech unproved."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_count, require_ordered
from tools.radio_outgoing_call_trace_check import (
    CM_SERVICE_REQUEST, CM_SERVICE_ACCEPT, SETUP, CALL_PROCEEDING,
    TRAFFIC_ASSIGNMENT, ASSIGNMENT_COMPLETE, ALERTING, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, RELEASE, RELEASE_COMPLETE, RR_RELEASE,
    PCH, decode_called_digits,
)


CHECKPOINTS = (
    ('physical Send', re.compile(r'8210_call_physical: action=send')),
    ('Send decode', re.compile(r'8210_keypad_decoded: key=0e\b')),
    ('CM Service Request', CM_SERVICE_REQUEST), ('CM Service Accept', CM_SERVICE_ACCEPT),
    ('SETUP', SETUP), ('Call Proceeding', CALL_PROCEEDING),
    ('assignment', TRAFFIC_ASSIGNMENT),
    ('own traffic configuration', re.compile(r'TX packet type=02 payload=20 .*data=040002000271012fc10000010000000400000000')),
    ('Assignment Complete', ASSIGNMENT_COMPLETE), ('Alerting', ALERTING),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'8210_call_physical: action=end')),
    ('End decode', re.compile(r'8210_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('Release', RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_RELEASE),
    ('own release configuration', re.compile(r'TX packet type=02 payload=20 .*data=040000001117001a600000040000001400000001')),
    ('idle confirmation', re.compile(r'RX enqueue type=89 payload=8 .*data=0000000000000000')),
    ('return to paging', PCH),
)


def verify(text, number='1234567'):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    require_ordered(text, CHECKPOINTS, '8210 outgoing signaling')
    for label, pattern in (('SETUP', SETUP), ('assignment', TRAFFIC_ASSIGNMENT),
                           ('Connect Acknowledge', CONNECT_ACKNOWLEDGE), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'8210 exactly one {label}')
    setup = SETUP.search(text)
    data = bytes.fromhex(setup.group('data'))
    if len(data) != int(setup.group('length')) or decode_called_digits(data) != number:
        raise ValueError('SETUP number differs from physical number')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--number', default='1234567')
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'GSM service' in line or
                           'LAPDm' in line or 'PCH no-identity' in line or 'packet' in line or
                           'RX enqueue' in line or '8210_call_physical' in line or
                           '8210_keypad_decoded' in line or '[LUA ERROR]' in line)
        verify(text, args.number)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 outgoing FAIL: {error}\n')
    print(f'8210 outgoing signaling PASS for {args.number}; speech unproved')
