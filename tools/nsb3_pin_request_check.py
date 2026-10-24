"""Qualify physical NSB-3 PIN-request Off/On and linked cold challenge behavior."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_pin_check import check_card, check_trace as check_pin, PIN_PROMPT, PHONE_PROMPT
from tools.nsb3_menu_check import check_trace as check_phone, FRAME as MENU_FRAME
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_puk_check import check_link
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.sim_security_trace_check import statuses, padded_pin
from tools.nsk1_phonebook_roundtrip import frame_check

SETTINGS_KEYS = ('Scroll Down',) * 3 + ('Left Softkey / Menu', 'Scroll Down',
    'Left Softkey / Menu', 'Left Softkey / Menu', 'Keypad 1', 'Keypad 2',
    'Keypad 3', 'Keypad 4', 'Left Softkey / Menu')


def check_trace(text, phase):
    if phase not in range(4):
        raise ValueError('unknown PIN-request phase')
    if phase in (0, 3):
        check_pin(text, True, allowed_mutations=(0x26,) if phase == 0 else ())
    else:
        check_phone(text)
        if 'nsb3_pin_key:' in text or statuses(text, 0x20):
            raise ValueError('disabled cold card must not request or verify PIN')
    keys = SETTINGS_KEYS + ('Scroll Down' if phase == 0 else 'Scroll Up', 'Left Softkey / Menu')
    rows = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    expected = [(key, down) for key in keys for down in ('1', '0')] if phase in (0, 2) else []
    if rows != expected:
        raise ValueError('physical PIN-request settings contacts differ')
    commands = wire(text)
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated SIM file/Toolkit mutation')
    mutations = [row[:5] for row in commands if row[0] in (0x24, 0x26, 0x28, 0x2c)]
    wanted = [(0x26 if phase == 0 else 0x28, 0, 1, 8, padded_pin('1234'))] if phase in (0, 2) else []
    if mutations != wanted:
        raise ValueError('PIN-request credential APDU differs')
    for ins in (0x26, 0x28):
        if statuses(text, ins) != ([(0x9000, 3, 10)] if wanted and ins == wanted[0][0] else []):
            raise ValueError('PIN-request status differs')


def check_chain(runs):
    if len(runs) != 5:
        raise ValueError('requires seed, Off, Off-cold, On and On-cold directories')
    check_card((runs[0] / 'nvram/nsb3r4t/sim_card').read_bytes(), 3)
    for run in runs:
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
    for phase, run in enumerate(runs[1:]):
        check_link(runs[phase], run)
        check_trace((run / 'error.log').read_text(), phase)
        check_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(), 3, enabled=phase >= 2)
        frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
        if phase in (0, 2):
            frame_check(run / 'snap/nsb3_security_14.png', '8a8c6c68ea5d450f6e4a7fc928ef838f2389c43acb6649491edb4bf0e1d7ad2f')
        elif phase == 1:
            frame_check(run / 'snap/nsb3_sim_12.png', PHONE_PROMPT)
        else:
            frame_check(run / 'snap/nsb3_pin_prompt.png', PIN_PROMPT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-disabled', 'prepare-enabled', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('cold preparation requires source and destination')
            enabled = args.operation == 'prepare-enabled'
            prepare_reload(*args.runs, lambda card: check_card(card, 3, enabled=enabled))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PIN request: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own PIN-request contract; phone lock separate')


if __name__ == '__main__':
    main()
