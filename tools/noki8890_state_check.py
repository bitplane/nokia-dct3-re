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


def verify(text):
    if '[LUA ERROR]' in text or '8890_state: FAIL' in text:
        raise ValueError('state fixture did not complete')
    states = re.findall(r'8890_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if len(states) != 2 or [state[0] for state in states] != ['saved', 'restored']:
        raise ValueError('missing exact save/load snapshots')
    if states[0][1:] != states[1][1:]:
        raise ValueError('CPU/RAM/time did not restore exactly')
    verify_roundtrip(text, ('TX packet', 'RX enqueue', 'GSM service', 'sim_device:'), '8890 idle')
    if not re.search(r'state_replay: phase=restored event=end[^\n]*\n[\s\S]*'
                     r'8890_state_physical: key=Menu[\s\S]*8890_keypad_decoded: key=19\b', text):
        raise ValueError('missing post-load physical Menu decode')


def check_frames(directory):
    frames = {
        '8890_state_idle_reference.png': ((0, 8, 84, 48), '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
        '8890_state_idle_restored.png': ((0, 8, 84, 48), '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
        '8890_state_idle_menu.png': ((0, 0, 72, 16), 'da31a6b8a573a7211b4eb55ffd4a8c05795988230cc5d3acfdf190fea65fe4c0'),
    }
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
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'))
        check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 idle state FAIL: {error}\n')
    print('8890 idle exact restoration/protocol replay/physical Menu PASS')
