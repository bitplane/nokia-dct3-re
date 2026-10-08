"""Verify NSM-3 incoming paging, physical Answer/End and release."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

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
from tools.noki8210_call_radio_contract import channel_patterns

TRAFFIC_CONFIG, RELEASE_CONFIG = channel_patterns()

CHECKPOINTS = (
    ('registration release', REGISTRATION_RELEASE), ('IMSI page', IMSI_PAGE),
    ('Paging Response', PAGING_RESPONSE), ('contention UA', PAGING_CONTENTION_UA),
    ('Cipher Mode Command', CIPHER_MODE_COMMAND), ('MM Information', MM_INFORMATION),
    ('Cipher Mode Complete', CIPHER_MODE_COMPLETE), ('incoming SETUP', INCOMING_SETUP),
    ('own Call Confirmed', re.compile(r'GSM service uplink sapi=0 pd=03 message=08 length=5 data=8308150101')),
    ('Alerting', ALERTING),
    ('own traffic configuration', TRAFFIC_CONFIG),
    ('traffic SABM', TRAFFIC_SABM), ('traffic UA', TRAFFIC_UA),
    ('Assignment Complete', ASSIGNMENT_COMPLETE),
    ('physical Answer', re.compile(r'8210_incoming_physical: action=Call / Send')),
    ('Answer decode', re.compile(r'8210_keypad_decoded: key=0e\b')),
    ('Connect', CONNECT), ('Connect Acknowledge', CONNECT_ACKNOWLEDGE),
    ('physical End', re.compile(r'8210_incoming_physical: action=End')),
    ('End decode', re.compile(r'8210_keypad_decoded: key=0f\b')),
    ('Disconnect', DISCONNECT), ('network Release', NETWORK_RELEASE),
    ('Release Complete', RELEASE_COMPLETE), ('RR release', RR_CHANNEL_RELEASE),
    ('traffic release UA', TRAFFIC_RELEASE_UA),
    ('own release configuration', RELEASE_CONFIG),
    ('release confirmation', RELEASE_CONFIRMATION), ('idle PCH', IDLE_PCH),
)


def verify(text, *, configured_carrier=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    traffic, release = channel_patterns(configured_carrier)
    replacements = {'own traffic configuration': traffic, 'own release configuration': release}
    checkpoints = tuple((name, replacements.get(name, pattern)) for name, pattern in CHECKPOINTS)
    require_ordered(text, checkpoints, '8210 incoming signaling')
    for label, pattern in (('incoming SETUP', INCOMING_SETUP), ('Connect', CONNECT), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'exactly one {label}')


def check_frames(directory):
    def digest(name, crop):
        with Image.open(directory / name) as source:
            if source.size != (84, 48):
                raise ValueError('unexpected handset frame geometry')
            return hashlib.sha256(source.convert('L').crop(crop).tobytes()).hexdigest()
    # Static caller text: exclude icons and softkey rows.
    if digest('8210_incoming_ringing.png', (0, 8, 84, 32)) != (
            'cf1063a9a4b3d131613ffc53861064245cf734f82bbc2387f598912007eb5180'):
        raise ValueError('missing reviewed caller 5551234')
    if digest('8210_after_incoming_call.png', (15, 0, 69, 16)) != (
            '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'):
        raise ValueError('missing reviewed registered idle after release')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--frames', type=Path)
    parser.add_argument('--configured-carrier', action='store_true')
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if 'dsp_hle:' in line or
                           'RX enqueue' in line or '8210_incoming_physical' in line or
                           '8210_keypad_decoded' in line or '[LUA ERROR]' in line)
        verify(text, configured_carrier=args.configured_carrier)
        if args.frames:
            check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 incoming FAIL: {error}\n')
    print('8210 incoming physical Answer/End and CC/RR signaling PASS; speech unproved')
