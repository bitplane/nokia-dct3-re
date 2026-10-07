"""Verify NSM-2 active-call restoration and physical release, not native speech."""
import argparse
from pathlib import Path
import re
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_state_roundtrip import verify_roundtrip
from tools.noki8850_outgoing_call_check import verify as verify_call, verify_frames
from tools.noki8850_sms_check import verify as verify_sms, verify_frame as verify_sms_frame


def verify(text, *, sms=False, storage=None):
    if '[LUA ERROR]' in text or '8850_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    states = re.findall(r'8850_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('architectural state did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'),
                     '8850 SMS' if sms else '8850 active call')
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
    verify_call(text)
    if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                     r'8850_call_physical: action=end', text):
        raise ValueError('missing post-load physical call release')


def check_frames(directory, *, sms=False):
    if sms:
        verify_sms_frame(directory / '8850_sms_read_4.png')
    else:
        verify_frames(directory)
    with Image.open(directory / '8850_state_call_reference.png') as source:
        reference = source.convert('L')
    with Image.open(directory / '8850_state_call_restored.png') as source:
        restored = source.convert('L')
    if reference.size != (84, 48) or restored.size != reference.size or (
            reference.tobytes() != restored.tobytes()):
        raise ValueError('connected-screen pixels did not replay exactly')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    parser.add_argument('--sms', action='store_true')
    parser.add_argument('--storage', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), sms=args.sms,
               storage=args.storage.read_bytes() if args.storage else None)
        check_frames(args.frames, sms=args.sms)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8850 call restoration FAIL: {error}\n')
    print('8850 exact restoration/protocol replay/physical continuation PASS; native speech unproved')
