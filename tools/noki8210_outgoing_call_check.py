"""Check own NSM-3 physical outgoing CC/RR lifecycle; speech unproved."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_call_lifecycle_common import require_count, require_ordered
from tools.noki8210_call_radio_contract import channel_patterns
from tools.radio_outgoing_call_trace_check import (
    CM_SERVICE_REQUEST, CM_SERVICE_ACCEPT, SETUP, CALL_PROCEEDING,
    TRAFFIC_ASSIGNMENT, ASSIGNMENT_COMPLETE, ALERTING, CONNECT,
    CONNECT_ACKNOWLEDGE, DISCONNECT, RELEASE, RELEASE_COMPLETE, RR_RELEASE,
    PCH, decode_called_digits,
)

TRAFFIC_CONFIG, RELEASE_CONFIG = channel_patterns()
FRAMES = {
    '8210_dialed_number.png': ((0, 0, 84, 24), '208fb985f2324bfadd7c10b6c0a5f0bb685001626499703dfe337045335de881'),
    # These Call 1 / registered-operator crops match the own state gate.
    '8210_outgoing_call.png': ((0, 0, 60, 16), 'cccb3b253638861cd041b9609851be0516cbde18e2118b625a2c34ad0cdd779e'),
    '8210_after_outgoing_call.png': ((15, 0, 69, 16), '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'),
}


CHECKPOINTS = (
    ('physical Send', re.compile(r'8210_call_physical: action=send')),
    ('Send decode', re.compile(r'8210_keypad_decoded: key=0e\b')),
    ('CM Service Request', CM_SERVICE_REQUEST), ('CM Service Accept', CM_SERVICE_ACCEPT),
    ('SETUP', SETUP), ('Call Proceeding', CALL_PROCEEDING),
    ('assignment', TRAFFIC_ASSIGNMENT),
    ('own traffic configuration', TRAFFIC_CONFIG),
    ('Assignment Complete', ASSIGNMENT_COMPLETE), ('Alerting', ALERTING),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'8210_call_physical: action=end')),
    ('End decode', re.compile(r'8210_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('Release', RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_RELEASE),
    ('own release configuration', RELEASE_CONFIG),
    ('idle confirmation', re.compile(r'RX enqueue type=89 payload=8 .*data=0000000000000000')),
    ('return to paging', PCH),
)


def verify(text, number='1234567', *, configured_carrier=False, dcs1800=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    traffic, release = channel_patterns(configured_carrier, dcs1800=dcs1800)
    replacements = {'own traffic configuration': traffic, 'own release configuration': release}
    checkpoints = tuple((name, replacements.get(name, pattern)) for name, pattern in CHECKPOINTS)
    require_ordered(text, checkpoints, '8210 outgoing signaling')
    for label, pattern in (('SETUP', SETUP), ('assignment', TRAFFIC_ASSIGNMENT),
                           ('Connect Acknowledge', CONNECT_ACKNOWLEDGE), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'8210 exactly one {label}')
    setup = SETUP.search(text)
    data = bytes.fromhex(setup.group('data'))
    if len(data) != int(setup.group('length')) or decode_called_digits(data) != number:
        raise ValueError('SETUP number differs from physical number')


def check_frames(directory):
    for name, (crop, expected) in FRAMES.items():
        with Image.open(directory / name) as source:
            digest = hashlib.sha256(source.convert('L').crop(crop).tobytes()).hexdigest()
            if source.size != (84, 48) or digest != expected:
                raise ValueError('missing reviewed outgoing call presentation: ' + name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--number', default='1234567')
    parser.add_argument('--configured-carrier', action='store_true')
    parser.add_argument('--dcs1800', action='store_true')
    parser.add_argument('--frames', type=Path)
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'GSM service' in line or
                           'LAPDm' in line or 'PCH no-identity' in line or 'packet' in line or
                           'RX enqueue' in line or '8210_call_physical' in line or
                           '8210_keypad_decoded' in line or '[LUA ERROR]' in line)
        verify(text, args.number, configured_carrier=args.configured_carrier,
               dcs1800=args.dcs1800)
        if args.frames:
            check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 outgoing FAIL: {error}\n')
    print(f'8210 outgoing signaling PASS for {args.number}; speech unproved')
