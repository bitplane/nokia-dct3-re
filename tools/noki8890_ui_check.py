"""Check NSB-6 SIM reads, physical security/menu inputs and reviewed pixels."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8890_staged_check import verify as verify_uploads


def verify(text):
    verify_uploads(text, runtime=True, selftest=True)
    for fid in ('2fe2', '6fae', '6f38', '6f07'):
        if not re.search(rf'read-binary fid={fid}\b', text):
            raise ValueError(f'missing firmware SIM read {fid}')
    records = set(int(x, 16) for x in re.findall(
        r'header cla=a0 ins=b2 p1=([0-9a-f]{2}) p2=04 p3=20 selected=6f3a', text))
    if not set(range(1, 51)) <= records:
        raise ValueError('not all 50 ADN records read')
    events = [('8890_security_physical: key=' + key, decoded) for key, decoded in (
        ('Keypad 1', '01'), ('Keypad 2', '02'), ('Keypad 3', '03'),
        ('Keypad 4', '04'), ('Keypad 5', '05'), ('Menu', '19'))]
    events += [(f'8890_navigation_physical: press={i}', '19') for i in range(1, 4)]
    cursor = 0
    for index, (marker, decoded) in enumerate(events):
        position = text.find(marker, cursor)
        if position < 0:
            raise ValueError(f'missing physical event {marker}')
        end = text.find(events[index + 1][0], position + len(marker)) if index + 1 < len(events) else len(text)
        if end < 0 or not re.search(rf'8890_keypad_decoded: key={decoded}\b', text[position:end]):
            raise ValueError(f'physical event did not decode: {marker}')
        cursor = end


def check_frames(directory):
    # Stable title/list rows: exclude animated icon and right scrollbar.
    expected = {
        '8890_navigation_2.png': 'da31a6b8a573a7211b4eb55ffd4a8c05795988230cc5d3acfdf190fea65fe4c0',
        '8890_navigation_3.png': 'a9823a39964f3294472660eb87248afca6bac9228b2f1b98d8806b19bc28662b',
    }
    for filename, digest in expected.items():
        with Image.open(directory / filename) as frame:
            actual = hashlib.sha256(frame.convert('L').crop((0, 0, 72, 16)).tobytes()).hexdigest()
            if frame.size != (84, 48) or actual != digest:
                raise ValueError(f'reviewed firmware UI frame mismatch: {filename}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'))
        check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 SIM/UI FAIL: {error}\n')
    print('8890 SIM/physical security/menu PASS; network and storage writes unproved')
