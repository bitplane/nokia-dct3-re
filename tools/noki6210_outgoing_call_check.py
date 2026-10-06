"""Own NPE-3 physical outgoing CC/RR lifecycle; speech remains unproved."""
import re
from tools.radio_call_lifecycle_common import require_count, require_ordered
from tools.radio_outgoing_call_trace_check import (
    CM_SERVICE_REQUEST, CM_SERVICE_ACCEPT, SETUP, CALL_PROCEEDING,
    TRAFFIC_ASSIGNMENT, ASSIGNMENT_COMPLETE, ALERTING, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, RELEASE, RELEASE_COMPLETE, RR_RELEASE,
    PCH, decode_called_digits,
)

CHECKPOINTS = (
    ('physical Send', re.compile(r'6210_call_physical: action=send')),
    ('Send decode', re.compile(r'6210_keypad_decoded: key=0e\b')),
    ('CM Service Request', CM_SERVICE_REQUEST), ('CM Service Accept', CM_SERVICE_ACCEPT),
    ('SETUP', SETUP), ('Call Proceeding', CALL_PROCEEDING),
    ('assignment', TRAFFIC_ASSIGNMENT),
    ('own traffic configuration', re.compile(r'TX packet type=02 payload=24 .*data=040002000271012fc1000001000000040000000000000000')),
    ('Assignment Complete', ASSIGNMENT_COMPLETE), ('Alerting', ALERTING),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'6210_call_physical: action=end')),
    ('End decode', re.compile(r'6210_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('Release', RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_RELEASE),
    ('own release configuration', re.compile(r'TX packet type=02 payload=24 .*data=040000001117001a60000023000000140000000100000000')),
    ('idle confirmation', re.compile(r'RX enqueue type=89 payload=8 .*data=0000000000000000')),
    ('return to paging', PCH),
)


def verify(text, number='1234567'):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    require_ordered(text, CHECKPOINTS, '6210 outgoing signaling')
    for label, pattern in (('SETUP', SETUP), ('assignment', TRAFFIC_ASSIGNMENT),
                           ('Connect Acknowledge', CONNECT_ACKNOWLEDGE), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'6210 exactly one {label}')
    setup = SETUP.search(text)
    data = bytes.fromhex(setup.group('data'))
    if len(data) != int(setup.group('length')) or decode_called_digits(data) != number:
        raise ValueError('SETUP number differs from physical digits')
