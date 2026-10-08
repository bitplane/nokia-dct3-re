"""Verify NSM-3 research idle restoration and physical continuation."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_state_roundtrip import verify_roundtrip
from tools.noki8210_outgoing_call_check import verify as verify_call
from tools.radio_outgoing_call_trace_check import CONNECT_ACKNOWLEDGE, DISCONNECT
from tools.noki8210_incoming_sms_check import verify as verify_sms
from tools.noki8210_incoming_sms_check import verify_frame as verify_sms_frame


def verify(text, *, call=False, sms=False, storage=None, sip_cancel=False):
    if sip_cancel and (call or sms):
        raise ValueError('SIP cancellation continuation requires an idle save')
    if '[LUA ERROR]' in text or '8210_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    states = re.findall(r'8210_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('architectural state did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'),
                     '8210 SMS' if sms else '8210 active call' if call else '8210 idle')
    if sms:
        if storage is None:
            raise ValueError('SMS restoration requires persistent SIM storage')
        verify_sms(text, storage)
        before_save = text.split('8210_state: event=saved', 1)[0]
        if ('sim_device: update fid=6f3c record=1 length=176' not in before_save or
                'LAPDm service Channel Release acknowledged' not in before_save):
            raise ValueError('save was not after delivered SMS storage and release')
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                         r'8210_sms_physical: action=read_2', text):
            raise ValueError('missing post-load physical SMS read')
        return
    if call:
        verify_call(text)
        before_save = text.split('8210_state: event=saved', 1)[0]
        if not CONNECT_ACKNOWLEDGE.search(before_save) or DISCONNECT.search(before_save):
            raise ValueError('save was not inside an established active call')
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                         r'8210_call_physical: action=end', text):
            raise ValueError('missing post-load physical call release')
        return
    if sip_cancel:
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                         r'8210_sip_cancel: physical Exit[\s\S]*'
                         r'8210_keypad_decoded: key=1a\b', text):
            raise ValueError('missing post-load physical SIP notification dismissal')
        return
    if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                     r'8210_state_physical: key=Menu[\s\S]*'
                     r'8210_keypad_decoded: key=19\b', text):
        raise ValueError('missing post-load physical Menu input')


def check_frames(directory, *, call=False, sms=False, sip_cancel=False):
    if sip_cancel and (call or sms):
        raise ValueError('SIP cancellation frames require an idle save')
    def read(name):
        with Image.open(directory / name) as source:
            if source.size != (84, 48):
                raise ValueError('unexpected handset frame geometry')
            return source.convert('L')
    scenario = 'sms' if sms else 'call' if call else 'idle'
    reference = read('8210_state_' + scenario + '_reference.png')
    restored = read('8210_state_' + scenario + '_restored.png')
    if reference.tobytes() != restored.tobytes():
        raise ValueError('saved-screen pixels did not replay exactly')
    if sms:
        verify_sms_frame(directory / '8210_sms_read_2.png')
        return
    if call and hashlib.sha256(reference.crop((0, 0, 60, 16)).tobytes()).hexdigest() != (
            'cccb3b253638861cd041b9609851be0516cbde18e2118b625a2c34ad0cdd779e'):
        raise ValueError('missing reviewed active-call presentation')
    operator = read('8210_state_call_released.png') if call else reference
    if hashlib.sha256(operator.crop((15, 0, 69, 16)).tobytes()).hexdigest() != (
            '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'):
        raise ValueError('missing reviewed registered operator')
    if not call and not sip_cancel and hashlib.sha256(read('8210_state_idle_menu.png').crop((0, 0, 72, 16)).tobytes()).hexdigest() != (
            '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'):
        raise ValueError('missing reviewed Messages menu')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    scenario = parser.add_mutually_exclusive_group()
    scenario.add_argument('--call', action='store_true')
    scenario.add_argument('--sms', action='store_true')
    parser.add_argument('--storage', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), call=args.call, sms=args.sms,
               storage=args.storage.read_bytes() if args.storage else None)
        check_frames(args.frames, call=args.call, sms=args.sms)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 restoration FAIL: {error}\n')
    print('8210 exact ' + ('delivered-SMS' if args.sms else 'active-call' if args.call else 'idle') +
          ' restoration/replay/physical continuation PASS; native DSP unproved')
