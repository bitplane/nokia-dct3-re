"""Physical NSB-3 PIN2 exhaustion and cold PUK2 recovery contract."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_pin2_change_check import check_pin2_card, check_trace as check_change, MENU_FRAME
from tools.nsb3_pin_change_check import CHANGE_KEYS
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import padded_pin, statuses
from tools.nsk1_phonebook_roundtrip import frame_check


def check_recovery(text, wrong=False, blocked=False):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts PUK2 qualification')
    verifies = () if wrong else ((0x20, 0, 2, 8, padded_pin('8765')),)
    check_pin(text, True, allowed_mutations=(0x2c,), additional_verifies=verifies)
    keys = CHANGE_KEYS[:12] + ('Scroll Down', 'Left Softkey / Menu')
    puk = '87654322' if wrong else '87654321'
    for digits in (puk, '8765', '8765'):
        keys += tuple('Keypad ' + digit for digit in digits) + ('Left Softkey / Menu',)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical PUK2/new/repeat sequence differs')
    commands = wire(text)
    credentials = [row[:5] for row in commands if row[0] in (0x20, 0x24, 0x26, 0x28, 0x2c)]
    if credentials != [(0x20, 0, 1, 8, padded_pin('1234')),
                       (0x2c, 0, 2, 16, puk.encode() + padded_pin('8765'))] + list(verifies):
        raise ValueError('requires PIN1 startup, exact UNBLOCK CHV2 then replacement VERIFY2')
    if statuses(text, 0x2c) != [(0x9840 if blocked else 0x9804 if wrong else 0x9000, 3, 10)]:
        raise ValueError('PUK2 recovery failed')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated SIM mutation')


def check_chain(runs):
    if len(runs) != 6:
        raise ValueError('requires seed/three-rejections/recovery/cold-restore')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        attempts = 3 - index if 1 <= index <= 3 else 3
        check_pin2_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(), index == 4, attempts)
        if not index:
            continue
        check_link(runs[index - 1], run)
        text = (run / 'error.log').read_text()
        if index <= 3:
            check_change(text, wrong=True, blocked=index == 3)
            frame_check(run / 'snap/nsb3_security_29.png',
                        '5e2f7fb2de8b983abd2f4cb3f6dc4591bc810b859df1ce6d5f89e8fe675e68d8'
                        if index == 3 else
                        'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e')
        elif index == 4:
            check_recovery(text)
            frame_check(run / 'snap/nsb3_security_14.png',
                        '5e2f7fb2de8b983abd2f4cb3f6dc4591bc810b859df1ce6d5f89e8fe675e68d8')
            frame_check(run / 'snap/nsb3_security_33.png',
                        'dd6eba3e2fe5e7bdfbc0fc187d2dc63475c1227911a0deeaa4be70546075a0c4')
        else:
            check_change(text, restore=True)
            frame_check(run / 'snap/nsb3_security_29.png',
                        'd145e1eb7e1008b3271c709e01b483fdb017b2eb9e29e80b3f545da93ccff14f')
        frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-last-wrong', 'prepare-recovery', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires source and cold destination')
            attempts = 1 if args.operation == 'prepare-last-wrong' else 0
            prepare_reload(*args.runs, lambda card: check_pin2_card(card, attempts=attempts))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PUK2: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' physical PIN2 exhaustion/PUK2/cold restore')


if __name__ == '__main__':
    main()
