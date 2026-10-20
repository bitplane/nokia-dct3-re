#!/usr/bin/env python3
"""Verify own NAM-2 security-input replay, not unattended idle or fitted flash."""

import argparse
import hashlib
from pathlib import Path
import re
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nam2_c3_checkpoint_check import FRAMES, check_pmm
from tools.nam2_v521_flash_contract import inspect


KEYS = ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu']
STATE = re.compile(r'nam2_security_state: event=(\w+) pc=([0-9a-f]{8}) sp=([0-9a-f]{8}) '
                   r'ram=([0-9a-f]{8}) cpu=([0-9a-f,]+) t=([0-9.]+)')


def check_log(text):
    if re.search(r'\[LUA ERROR\]|Error in|FAIL|architectural state differs', text):
        raise ValueError('fixture error or incomplete replay')
    states = list(STATE.finditer(text))
    if [match[1] for match in states] != ['saved', 'reference_end', 'restored', 'replay_end']:
        raise ValueError('save/replay observation order differs')
    for match in states:
        if not re.fullmatch(r'[0-9a-f]{8}(?:,[0-9a-f]{8}){36}', match[5]):
            raise ValueError('incomplete CPU register observation')
    if states[0].groups()[1:] != states[2].groups()[1:]:
        raise ValueError('save restoration differs')
    if states[1].groups()[1:] != states[3].groups()[1:]:
        raise ValueError('physical continuation differs')
    if float(states[0][6]) != 7.75 or float(states[1][6]) != 11:
        raise ValueError('security checkpoint timing differs')
    contacts = list(re.finditer(r'nam2_security_state_key: phase=(\w+) index=(\d+) key=(.*?) t=([0-9.]+)', text))
    expected = [(phase, str(index + 1), key, 8 + index * 0.5)
                for phase in ('reference', 'replay') for index, key in enumerate(KEYS)]
    actual = [(match[1], match[2], match[3], float(match[4])) for match in contacts]
    if actual != expected:
        raise ValueError('physical contacts differ')
    for begin, end, phase_contacts in ((states[0], states[1], contacts[:6]),
                                       (states[2], states[3], contacts[6:])):
        if not all(begin.end() < contact.start() < end.start() for contact in phase_contacts):
            raise ValueError('physical contact outside replay interval')
    marker = 'nam2_security_state: result=pass physical_replay=1 idle_claim=0'
    if text.count(marker) != 1 or text.index(marker) < states[-1].end():
        raise ValueError('terminal replay result absent or out of order')


def check_frames(directory):
    expected = FRAMES['nam2_v521_security_6.png']
    for phase in ('reference', 'replay'):
        with Image.open(directory / f'nam2_security_state_{phase}.png') as source:
            frame = source.convert('RGB')
        if frame.size != (96, 65) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
            raise ValueError(f'{phase}: reviewed Code accepted pixels differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        source = args.image.read_bytes()
        inspect(source)
        check_log((args.run / 'error.log').read_text())
        check_frames(args.run / 'snap')
        check_pmm(source, (args.run / 'nvram/nam2c3_1/flash').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'NAM-2 security replay: FAIL: {error}\n')
    print('NAM-2 security replay: PASS exact architecture, physical input, accepted pixels and own PMM; idle unproved')


if __name__ == '__main__':
    main()
