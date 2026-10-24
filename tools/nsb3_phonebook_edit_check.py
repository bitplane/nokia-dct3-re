"""Own NSB-3 physical SIM-contact edit and independently cold Details."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_phonebook_check import READ_KEYS, check_security_menu, expected_record, prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import statuses
from tools.nsk1_phonebook_roundtrip import frame_check

EDIT_KEYS = READ_KEYS + ('Left Softkey / Menu', 'Left Softkey / Menu',
    'Right Softkey / C', 'Keypad 3', 'Left Softkey / Menu',
    'Right Softkey / C', 'Keypad 6', 'Left Softkey / Menu', 'Left Softkey / Menu')


def edited_record():
    record = bytearray(expected_record())
    record[0] = ord('D')
    record[22] = 0xf6
    return bytes(record)


def expected_card(edited=True):
    card = bytearray(make_current_profile(False))
    card[:32] = edited_record() if edited else expected_record()
    return bytes(card)


def check_edited_card(card):
    if card != expected_card():
        raise ValueError('edited D/12346 card differs beyond qualified ADN fields')


def check_trace(text, readback=False):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts contact edit qualification')
    check_security_menu(text)
    keys = READ_KEYS if readback else EDIT_KEYS
    contacts = re.findall(r'nsb3_phonebook_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical edit/cold-read contacts differ')
    commands = wire(text)
    updates = [row for row in commands if row[0] == 0xdc]
    if updates != ([] if readback else [(0xdc, 1, 4, 32, edited_record(), 0x6f3a)]):
        raise ValueError('requires exact D/12346 UPDATE then read-only cold Details')
    if statuses(text, 0xdc) != ([] if readback else [(0x9000, 3, 10)]):
        raise ValueError('contact edit status differs')
    if not any(row[:4] == (0xb2, 1, 4, 32) and row[5] == 0x6f3a for row in commands):
        raise ValueError('ADN read missing')
    if any(row[0] in (4, 0x44, 0xd6, 0x20, 0x24, 0x26, 0x28, 0x2c, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated credential/file/Toolkit mutation')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires original-contact source/edit/cold-read')
    if (runs[0] / 'nvram/nsb3r4t/sim_card').read_bytes() != expected_card(False):
        raise ValueError('requires exact original A/12345 laboratory card')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if index:
            check_link(runs[index - 1], run)
            check_edited_card((run / 'nvram/nsb3r4t/sim_card').read_bytes())
            check_trace((run / 'error.log').read_text(), index == 2)
            if index == 1:
                frame_check(run / 'snap/nsb3_phonebook_14.png',
                            '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231')
            else:
                frame_check(run / 'snap/nsb3_phonebook_04.png',
                            'baf2f39cdb75da0a171b26470d3ceb10c1382984d4c640d98496e373d73b7411')
                frame_check(run / 'snap/nsb3_phonebook_05.png',
                            '9b88b9c2b87def776295d6efba86b5728617a4f083fef151917a88721da7b7c2')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-cold', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires edited source and cold destination')
            prepare_reload(*args.runs, check_edited_card)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 contact edit: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own physical ADN edit/cold Details')


if __name__ == '__main__':
    main()
