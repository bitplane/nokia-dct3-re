"""Own NSB-3 physical SIM-contact deletion and independently cold empty search."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_phonebook_check import READ_KEYS, check_security_menu, check_card, prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import statuses
from tools.nsk1_phonebook_roundtrip import frame_check

ERASE_KEYS = READ_KEYS + ('Left Softkey / Menu', 'Scroll Down',
                         'Left Softkey / Menu', 'Left Softkey / Menu')
EMPTY_KEYS = READ_KEYS[:4]


def check_empty_card(card):
    if card != make_current_profile(False):
        raise ValueError('empty laboratory card differs beyond erased ADN record')


def check_trace(text, empty=False):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts storage qualification')
    check_security_menu(text)
    keys = EMPTY_KEYS if empty else ERASE_KEYS
    contacts = re.findall(r'nsb3_phonebook_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical erase/empty-search contacts differ')
    commands = wire(text)
    updates = [row for row in commands if row[0] == 0xdc]
    if updates != ([] if empty else [(0xdc, 1, 4, 32, b'\xff' * 32, 0x6f3a)]):
        raise ValueError('requires one erased ADN record, then read-only cold search')
    if statuses(text, 0xdc) != ([] if empty else [(0x9000, 3, 10)]):
        raise ValueError('ADN erase status differs')
    if not any(row[:4] == (0xb2, 1, 4, 32) and row[5] == 0x6f3a for row in commands):
        raise ValueError('ADN record-one read missing')
    if any(row[0] in (4, 0x44, 0xd6, 0x20, 0x24, 0x26, 0x28, 0x2c, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated credential/file/Toolkit mutation')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires saved-contact source/erase/cold-empty')
    before = (runs[0] / 'nvram/nsb3r4t/sim_card').read_bytes()
    check_card(before)
    expected = b'\xff' * 32 + before[32:]
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if not index:
            continue
        check_link(runs[index - 1], run)
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        if card != expected:
            raise ValueError('deletion/cold search changed unrelated card bytes')
        check_empty_card(card)
        check_trace((run / 'error.log').read_text(), index == 2)
        if index == 1:
            frame_check(run / 'snap/nsb3_phonebook_09.png',
                        '922da30495f7290e0ab6f9b128f85f823ee66274495945c30365d8b6497e74d8')
        else:
            frame_check(run / 'snap/nsb3_phonebook_03.png',
                        'a75564149aebba9ecd1ebc8ef1b58c45aa56dd95431a8078122d2027cda70d04')
            frame_check(run / 'snap/nsb3_phonebook_04.png',
                        'ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-empty', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires erased source and cold destination')
            prepare_reload(*args.runs, check_empty_card)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 contact erase: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own physical ADN erase/cold-empty contract')


if __name__ == '__main__':
    main()
