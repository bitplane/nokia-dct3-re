"""Own NSB-3 physical PIN2 change and independently cold authenticated restore."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_pin_check import check_card, check_trace as check_pin
from tools.nsb3_pin_change_check import CHANGE_KEYS
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import padded_pin, statuses, CHV_OFFSET, ATTEMPTS_OFFSET
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME


def check_pin2_card(card, changed=False, attempts=3):
    canonical = bytearray(card)
    if len(canonical) <= ATTEMPTS_OFFSET + 1 or canonical[ATTEMPTS_OFFSET + 1] != attempts:
        raise ValueError('PIN2 retry count differs')
    canonical[ATTEMPTS_OFFSET + 1] = 3
    if changed:
        if canonical[CHV_OFFSET + 8:CHV_OFFSET + 16] != padded_pin('8765'):
            raise ValueError('changed PIN2 required')
        canonical[CHV_OFFSET + 8:CHV_OFFSET + 16] = padded_pin('5678')
    check_card(bytes(canonical), 3)


def check_trace(text, restore=False, wrong=False, blocked=False):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts credential qualification')
    check_pin(text, True, allowed_mutations=(0x24,))
    old, new = ('8765', '5678') if restore else ('5678', '8765')
    if wrong:
        old = '5679'
    keys = CHANGE_KEYS[:12] + ('Scroll Down', 'Left Softkey / Menu')
    for digits in (old, new, new):
        keys += tuple('Keypad ' + digit for digit in digits) + ('Left Softkey / Menu',)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical PIN2 item/current/new/repeat contacts differ')
    commands = wire(text)
    credentials = [row[:5] for row in commands if row[0] in (0x20, 0x24, 0x26, 0x28, 0x2c)]
    if credentials != [(0x20, 0, 1, 8, padded_pin('1234')),
                       (0x24, 0, 2, 16, padded_pin(old) + padded_pin(new))]:
        raise ValueError('requires PIN1 startup then exact PIN2 CHANGE CHV')
    if statuses(text, 0x24) != [(0x9840 if blocked else 0x9804 if wrong else 0x9000, 3, 10)]:
        raise ValueError('PIN2 change failed')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated SIM mutation')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires seed/change/cold-restore directories')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        check_pin2_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(), index == 1)
        if index:
            check_link(runs[index - 1], run)
            check_trace((run / 'error.log').read_text(), index == 2)
            frame_check(run / 'snap/nsb3_security_29.png',
                        'd145e1eb7e1008b3271c709e01b483fdb017b2eb9e29e80b3f545da93ccff14f')
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-change', 'prepare-restore', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires source and destination')
            prepare_reload(*args.runs, lambda card: check_pin2_card(card, args.operation == 'prepare-restore'))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PIN2: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own PIN2 contract; native radio remains unqualified')


if __name__ == '__main__':
    main()
