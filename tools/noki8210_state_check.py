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


def verify(text):
    if '[LUA ERROR]' in text or '8210_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    states = re.findall(r'8210_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('architectural state did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'), '8210 idle')
    if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                     r'8210_state_physical: key=Menu[\s\S]*'
                     r'8210_keypad_decoded: key=19\b', text):
        raise ValueError('missing post-load physical Menu input')


def check_frames(directory):
    def read(name):
        with Image.open(directory / name) as source:
            if source.size != (84, 48):
                raise ValueError('unexpected handset frame geometry')
            return source.convert('L')
    reference = read('8210_state_idle_reference.png')
    restored = read('8210_state_idle_restored.png')
    if reference.tobytes() != restored.tobytes():
        raise ValueError('saved-screen pixels did not replay exactly')
    if hashlib.sha256(reference.crop((15, 0, 69, 16)).tobytes()).hexdigest() != (
            '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'):
        raise ValueError('missing reviewed registered operator')
    if hashlib.sha256(read('8210_state_idle_menu.png').crop((0, 0, 72, 16)).tobytes()).hexdigest() != (
            '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'):
        raise ValueError('missing reviewed Messages menu')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'))
        check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 restoration FAIL: {error}\n')
    print('8210 exact idle restoration/replay/physical continuation PASS; native DSP unproved')
