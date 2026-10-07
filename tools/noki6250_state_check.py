"""Verify NHM-3 research-composition idle restoration, not factory boot."""
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
    if '[LUA ERROR]' in text or '6250_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    states = re.findall(r'6250_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('architectural state did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'), '6250 idle')
    if not re.search(r'state_replay: phase=restored event=end[\s\S]*'
                     r'6250_state_physical: key=Menu[\s\S]*'
                     r'6250_raw_matrix_key: value=06\b', text):
        raise ValueError('missing post-load physical Menu scan')


def check_frames(directory):
    def read(name):
        with Image.open(directory / name) as source:
            if source.size != (96, 60):
                raise ValueError('unexpected handset frame geometry')
            return source.convert('L')
    reference = read('6250_state_idle_reference.png')
    restored = read('6250_state_idle_restored.png')
    if reference.tobytes() != restored.tobytes():
        raise ValueError('saved-screen pixels did not replay exactly')
    if hashlib.sha256(reference.crop((15, 0, 81, 16)).tobytes()).hexdigest() != (
            '3fe7f6101004fbdb4634491764dcae956ab34e6701e88330f6cde4e781d8ab85'):
        raise ValueError('missing reviewed registered operator')
    if hashlib.sha256(read('6250_state_idle_menu.png').crop((0, 0, 84, 16)).tobytes()).hexdigest() != (
            '91f4829ae667137ccc27fa2e3a266161d4b6518d2ed6f10470abf7de6ce7e372'):
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
        parser.exit(1, f'6250 restoration FAIL: {error}\n')
    print('6250 research idle restoration/replay/physical continuation PASS; original PMM/native DSP unproved')
