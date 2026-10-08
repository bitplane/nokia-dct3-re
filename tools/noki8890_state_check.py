"""Check exact NSB-6 idle restoration, protocol replay and physical Menu."""
import argparse
import hashlib
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_state_roundtrip import verify_roundtrip
from tools.noki8890_outgoing_call_check import verify as verify_call
from tools.noki8890_incoming_sms_check import verify as verify_sms
from tools.noki8890_registration_check import verify as verify_registration


def verify(text, call=False, sms=False, storage=None, pcs1900=False, sip_cancel=False,
           configured_gsm900=False):
    if '[LUA ERROR]' in text or '8890_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    if pcs1900:
        verify_registration(text, pcs1900=True)
    if configured_gsm900:
        verify_registration(text, configured_gsm900=True)
    states = re.findall(r'8890_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('CPU/RAM/time did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'), '8890 idle')
    if sip_cancel:
        if call or sms:
            raise ValueError('SIP cancellation requires idle restoration')
        if not re.search(r'state_replay: phase=restored event=end[^\n]*\n[\s\S]*'
                         r'8890_sip_cancel: physical Exit[\s\S]*8890_keypad_decoded: key=1a\b', text):
            raise ValueError('missing post-load physical SIP notification dismissal')
        return
    if sms:
        if storage is None:
            raise ValueError('SMS restoration requires persistent SIM storage')
        verify_sms(text, storage, pcs1900=pcs1900)
        if not re.search(r'state_replay: phase=restored event=end[\s\S]*8890_sms_physical: action=read_2', text):
            raise ValueError('missing post-load physical SMS read')
        return
    if call:
        verify_call(text, pcs1900=pcs1900, configured_gsm900=configured_gsm900)
        return
    if not re.search(r'state_replay: phase=restored event=end[^\n]*\n[\s\S]*'
                     r'8890_state_physical: key=Menu[\s\S]*8890_keypad_decoded: key=19\b', text):
        raise ValueError('missing post-load physical Menu decode')


def check_frames(directory, call=False, sms=False):
    frames = {
        '8890_state_idle_reference.png': ((0, 8, 84, 48), '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
        '8890_state_idle_restored.png': ((0, 8, 84, 48), '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
        '8890_state_idle_menu.png': ((0, 0, 72, 16), 'da31a6b8a573a7211b4eb55ffd4a8c05795988230cc5d3acfdf190fea65fe4c0'),
    }
    if call or sms:
        frames = ({'8890_state_call_released.png': frames['8890_state_idle_restored.png']}
                  if call else {'8890_sms_read_2.png': ((0, 0, 84, 24),
                      '426de6fc34ebd2112536e8f3245696c996f624abf6d6569ead2c8c0651b49635')})
    with Image.open(directory / '8890_state_idle_reference.png') as reference:
        with Image.open(directory / '8890_state_idle_restored.png') as restored:
            if reference.size != (84, 48) or restored.size != reference.size or reference.convert('L').tobytes() != restored.convert('L').tobytes():
                raise ValueError('reference/restored pixels differ')
    # Exclude the advancing idle clock and animated menu icon/scrollbar.
    for name, (region, expected) in frames.items():
        with Image.open(directory / name) as frame:
            actual = hashlib.sha256(frame.convert('L').crop(region).tobytes()).hexdigest()
            if frame.size != (84, 48) or actual != expected:
                raise ValueError(f'state UI frame mismatch: {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    scenario = parser.add_mutually_exclusive_group()
    scenario.add_argument('--call', action='store_true')
    scenario.add_argument('--sms', action='store_true')
    parser.add_argument('--storage', type=Path, help='persistent SIM image required for SMS')
    parser.add_argument('--pcs1900', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), args.call, args.sms,
               args.storage.read_bytes() if args.storage else None, args.pcs1900)
        check_frames(args.frames, args.call, args.sms)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 idle state FAIL: {error}\n')
    print('8890 exact restoration/protocol replay/physical continuation PASS')
