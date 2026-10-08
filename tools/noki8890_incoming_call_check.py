"""Verify NSB-6 paging, physical Answer/End and complete call release."""
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


def host_setup_pattern(caller):
    if not caller or len(caller) > 20 or any(digit not in '0123456789' for digit in caller):
        raise ValueError('host caller must contain 1..20 decimal digits')
    digits = caller + ('f' if len(caller) % 2 else '')
    bcd = bytes(int(digits[index + 1] + digits[index], 16)
                for index in range(0, len(digits), 2))
    # One unsegmented SAPI-0 SETUP: bearer capability, signal, calling number.
    setup = bytes.fromhex('030504046002008134015c') + bytes([len(bcd) + 1, 0x81]) + bcd
    return re.compile(r'RX enqueue type=80 payload=34 .*data=80[0-9a-f]{18}'
                      r'03[0-9a-f]{2}' + f'{len(setup) * 4 + 1:02x}' + setup.hex())


def verify(text, *, pcs1900=False, caller=None, configured_gsm900=False):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    if pcs1900 and configured_gsm900:
        raise ValueError('PCS1900 and configured GSM900 are distinct compositions')
    checkpoints = CHECKPOINTS
    if configured_gsm900:
        verify_registration(text, configured_gsm900=True)
        checkpoints = tuple((label, re.compile(
            r'TX packet type=02 payload=20 .*data=041202000271012fc100003c0000000400000000'
            if label == 'own traffic configuration' else
            r'TX packet type=02 payload=20 .*data=041202001117001a6000003c0000001400000001'
            if label == 'own release configuration' else pattern.pattern))
            for label, pattern in checkpoints)
    if caller is not None:
        checkpoints = tuple((label, host_setup_pattern(caller)
                             if label == 'incoming SETUP' else pattern)
                            for label, pattern in checkpoints)
    if pcs1900:
        verify_registration(text, pcs1900=True)
        checkpoints = tuple((label, re.compile(
            r'TX packet type=02 payload=20 .*data=041202000271012fc10002580000000400000000'
            if label == 'own traffic configuration' else
            r'TX packet type=02 payload=20 .*data=041202001117001a600002580000001400000001'
            if label == 'own release configuration' else pattern.pattern))
            for label, pattern in checkpoints)
    require_ordered(text, checkpoints, '8890 incoming signaling')
    setup_pattern = host_setup_pattern(caller) if caller is not None else INCOMING_SETUP
    for label, pattern in (('incoming SETUP', setup_pattern), ('Connect', CONNECT), ('Disconnect', DISCONNECT)):
        require_count(text, pattern, 1, f'exactly one {label}')


def check_host_frames(directory, caller):
    if caller != '447700900123':
        raise ValueError('reviewed host frames require caller 447700900123')
    expected = {
        '8890_incoming_ringing.png': '9502fbe9e5032d74c2a087932ea417e3cfa1d1104746ccfbe420ca55efd36cc1',
        '8890_after_incoming_call.png': '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de',
    }
    # Exclude only the advancing top-row clock, not caller/softkeys/idle text.
    for name, digest in expected.items():
        with Image.open(directory / name) as frame:
            actual = hashlib.sha256(frame.convert('L').crop((0, 8, 84, 48)).tobytes()).hexdigest()
            if frame.size != (84, 48) or actual != digest:
                raise ValueError(f'reviewed incoming presentation mismatch: {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--pcs1900', action='store_true')
    parser.add_argument('--caller', help='require this host caller instead of the fixed laboratory caller')
    parser.add_argument('--frames', type=Path, help='check settled-idle host-call presentation')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), pcs1900=args.pcs1900, caller=args.caller)
        if args.frames:
            check_host_frames(args.frames, args.caller)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 incoming FAIL: {error}\n')
    print('8890 incoming physical Answer/End and CC/RR signaling PASS; speech unproved')
