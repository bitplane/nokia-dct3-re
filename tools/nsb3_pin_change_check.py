"""Qualify own NSB-3 menu PIN change, old-PIN rejection and cold new-PIN login."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_pin_check import check_card, check_trace as check_pin, PIN_PROMPT, WRONG_FRAME, PHONE_PROMPT, MENU_FRAME
from tools.nsb3_pin_request_check import SETTINGS_KEYS
from tools.nsb3_puk_check import check_link
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import statuses, padded_pin
from tools.nsk1_phonebook_roundtrip import frame_check

CHANGE_KEYS = SETTINGS_KEYS[:6] + ('Scroll Down',) * 5 + ('Left Softkey / Menu',) * 2
for _digits in ('1234', '4321', '4321'):
    CHANGE_KEYS += tuple('Keypad ' + digit for digit in _digits) + ('Left Softkey / Menu',)


def check_change(text):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts the credential qualification window')
    check_pin(text, True, allowed_mutations=(0x24,))
    rows = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if rows != [(key, down) for key in CHANGE_KEYS for down in ('1', '0')]:
        raise ValueError('physical Access codes/current/new/repeat sequence differs')
    commands = wire(text)
    credentials = [row[:5] for row in commands if row[0] in (0x20, 0x24, 0x26, 0x28, 0x2c)]
    if credentials != [(0x20, 0, 1, 8, padded_pin('1234')),
                       (0x24, 0, 1, 16, padded_pin('1234') + padded_pin('4321'))]:
        raise ValueError('requires ordered startup VERIFY then current/new CHANGE CHV')
    if statuses(text, 0x24) != [(0x9000, 3, 10)]:
        raise ValueError('CHANGE CHV status differs')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated SIM mutation')


def check_chain(runs):
    if len(runs) != 4:
        raise ValueError('requires seed/change/old-rejected/new-accepted directories')
    check_card((runs[0] / 'nvram/nsb3r4t/sim_card').read_bytes(), 3)
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if not index:
            continue
        check_link(runs[index - 1], run)
        text = (run / 'error.log').read_text()
        check_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(), 2 if index == 2 else 3, '4321')
        if index == 1:
            check_change(text)
            frame_check(run / 'snap/nsb3_security_28.png',
                        'e3d554448ecdac351002f8a3f94582448fc1f03cc5a9b3071e9b0e60272f018a')
        else:
            check_pin(text, index == 3, attempts_before=2 if index == 3 else 3,
                      pin='4321' if index == 3 else '1234')
            frame_check(run / 'snap/nsb3_pin_prompt.png', PIN_PROMPT)
            frame_check(run / 'snap/nsb3_pin_result.png', PHONE_PROMPT if index == 3 else WRONG_FRAME)
        if index in (1, 3):
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-old', 'prepare-new', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('cold preparation requires source and destination')
            attempts = 3 if args.operation == 'prepare-old' else 2
            prepare_reload(*args.runs, lambda card: check_card(card, attempts, '4321'))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PIN change: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own menu PIN-change contract; phone lock separate')


if __name__ == '__main__':
    main()
