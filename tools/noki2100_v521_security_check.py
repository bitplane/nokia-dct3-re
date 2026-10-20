#!/usr/bin/env python3
"""Validate bounded early physical security-editor interaction, not usable idle."""

import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_2100_pmm_profile import V521_SHA1

FRAMES = {
    'nam2_v521_security_before.png': 'b295728b818021342c6ac64148ac54c3be92f63a588c1607c325e9062935b491',
    'nam2_v521_security_5.png': 'ced78aa7c098ae337521429d1855e17e1f89f91c57cfe5bf0c59f669a098a522',
    'nam2_v521_security_6.png': '39545bd7cf0c050611a1caeb1bbc4d01baa78085aa4d7f387d892c9246a25bc1',
}


def check_input(text):
    if '[LUA ERROR]' in text:
        raise ValueError('physical fixture failed')
    if 'bootstrap completion exchanges=992 publications=3' not in text:
        raise ValueError('missing own v5.21 bootstrap')
    actions = re.findall(r'nam2_v521_security_physical: index=(\d+) key=(.*?) t=([0-9.]+)', text)
    expected = [(str(index + 1), name, 8 + index * 0.5) for index, name in enumerate(
        ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu'])]
    if [(index, name, float(time)) for index, name, time in actions] != expected:
        raise ValueError('physical key sequence/timing differs')
    routes = re.findall(r'keypad_route: state=(\d) before=[0-9a-f]+ after=([0-9a-f]+) '
                        r'mask=([0-9a-f]+) ctrl=([0-9a-f]+) pc=[0-9a-f]+ t=([0-9.]+)', text)
    for _, _, when in expected:
        eligible = [(state, int(after, 16), int(mask, 16), int(ctrl, 16))
                    for state, after, mask, ctrl, time in routes if when <= float(time) < when + 0.2]
        if not any(state == '1' and after & 1 and not mask & 1 for state, after, mask, ctrl in eligible):
            raise ValueError('physical contact lacks locally unmasked IRQ0')
        if not any(state == '0' and not after & 1 and ctrl & 4 for state, after, mask, ctrl in eligible):
            raise ValueError('physical contact lacks firmware IRQ0 service')


def check_frames(directory):
    for name, expected in FRAMES.items():
        with Image.open(directory / name) as source:
            frame = source.convert('RGB')
        if frame.size != (96, 65) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError(f'reviewed security frame differs: {name}')


def check_storage(image, flash):
    if hashlib.sha1(image).hexdigest() != V521_SHA1:
        raise ValueError('wrong acquired v5.21 source image')
    if len(flash) != len(image) or flash[:0x1F0000] != image[:0x1F0000]:
        raise ValueError('runtime executable is not the unchanged own v5.21 image')
    # PMM is firmware-owned and may be erased/rebuilt during this run. This
    # check establishes executable provenance, not persistent-storage validity.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        check_storage(args.image.read_bytes(), (args.run / 'nvram/noki2100_1/flash').read_bytes())
        check_input((args.run / 'error.log').read_text())
        check_frames(args.run / 'snap')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NAM-2 v5.21 security interaction: FAIL: {error}\n')
    print('NAM-2 v5.21: physical 12345/editor/rejection verified; usable idle and code acceptance unproved')


if __name__ == '__main__':
    main()
