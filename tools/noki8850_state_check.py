"""Verify NSM-2 active-call restoration and physical release, not native speech."""
import argparse
from pathlib import Path
import re
import sys
import hashlib
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_state_roundtrip import verify_roundtrip
from tools.noki8850_outgoing_call_check import verify as verify_call, verify_frames
from tools.noki8850_sms_check import verify as verify_sms, verify_frame as verify_sms_frame


def verify_architecture(text):
    if '[LUA ERROR]' in text or '8850_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    states = re.findall(r'8850_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('architectural state did not restore exactly')
    return states


def verify(text, *, sms=False, idle=False, storage=None, sip_cancel=False):
    verify_architecture(text)
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'),
                     '8850 SMS' if sms else '8850 active call')
    if sip_cancel:
        if not idle or sms:
            raise ValueError('SIP cancellation requires idle restoration')
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                         r'8850_sip_cancel: physical Exit[\s\S]*'
                         r'8850_keypad_decoded key=1a\b', text):
            raise ValueError('missing post-load physical SIP notification dismissal')
        if '8850_call_physical: action=send' in text:
            raise ValueError('idle fixture unexpectedly initiated a call')
        return
    if sms:
        if storage is None:
            raise ValueError('SMS restoration requires persistent SIM storage')
        verify_sms(text, storage)
        if text.count('PCH IMSI page transmitted channel=60') != 1 or (
                text.count('sim_device: update fid=6f3c record=1 length=176') != 2):
            raise ValueError('SMS restoration redelivered or rewrote the stored message')
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                         r'8850_sms_physical: action=read_4', text):
            raise ValueError('missing post-load physical SMS read')
        return
    if idle:
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                         r'8850_state_physical: key=Menu[\s\S]*'
                         r'8850_keypad_decoded key=19\b', text):
            raise ValueError('missing post-load physical Menu input')
        if '8850_call_physical: action=send' in text:
            raise ValueError('idle fixture unexpectedly initiated a call')
        return
    verify_call(text)
    if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                     r'8850_call_physical: action=end', text):
        raise ValueError('missing post-load physical call release')


def check_frames(directory, *, sms=False, idle=False, sip_cancel=False):
    if sip_cancel and (not idle or sms):
        raise ValueError('SIP cancellation requires idle frames')
    if sms:
        verify_sms_frame(directory / '8850_sms_read_4.png')
    elif not idle:
        verify_frames(directory)
    with Image.open(directory / '8850_state_call_reference.png') as source:
        reference = source.convert('L')
    with Image.open(directory / '8850_state_call_restored.png') as source:
        restored = source.convert('L')
    if reference.size != (84, 48) or restored.size != reference.size or (
            reference.tobytes() != restored.tobytes()):
        raise ValueError('saved-screen pixels did not replay exactly')
    if idle:
        if hashlib.sha256(reference.crop((15, 0, 69, 16)).tobytes()).hexdigest() != (
                '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'):
            raise ValueError('missing reviewed idle operator text')
        if sip_cancel:
            return
        with Image.open(directory / '8850_state_idle_menu.png') as source:
            menu = source.convert('L')
        if menu.size != (84, 48) or hashlib.sha256(
                menu.crop((0, 0, 72, 16)).tobytes()).hexdigest() != (
                '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'):
            raise ValueError('missing reviewed Messages menu')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    scenario = parser.add_mutually_exclusive_group()
    scenario.add_argument('--sms', action='store_true')
    scenario.add_argument('--idle', action='store_true')
    parser.add_argument('--storage', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), sms=args.sms, idle=args.idle,
               storage=args.storage.read_bytes() if args.storage else None)
        check_frames(args.frames, sms=args.sms, idle=args.idle)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8850 restoration FAIL: {error}\n')
    print('8850 exact restoration/protocol replay/physical continuation PASS; native speech unproved')
