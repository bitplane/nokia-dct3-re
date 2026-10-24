"""Own NSB-3 physical PIN2-protected FDN edit and independently cold read."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_fdn_record_check import READ_KEYS, FDN_OFFSET, expected_record, expected_card as original_card, check_card as check_original, check_record_trace
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME

EDIT_KEYS = READ_KEYS + ('Left Softkey / Menu',) + tuple('Keypad ' + digit for digit in '5678') + (
    'Left Softkey / Menu', 'Left Softkey / Menu', 'Left Softkey / Menu',
    'Right Softkey / C', 'Keypad 6', 'Left Softkey / Menu')


def edited_record():
    record = bytearray(expected_record())
    record[22] = 0xf6
    return bytes(record)


def expected_card():
    card = bytearray(original_card())
    card[FDN_OFFSET:FDN_OFFSET + 32] = edited_record()
    return bytes(card)


def check_card(card):
    if card != expected_card():
        raise ValueError('edited FDN card differs beyond final number digit')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires saved-contact source/edit/cold-read')
    check_original((runs[0] / 'nvram/nsb3r4t/sim_card').read_bytes())
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if index:
            check_link(runs[index - 1], run)
            check_card((run / 'nvram/nsb3r4t/sim_card').read_bytes())
            check_record_trace((run / 'error.log').read_text(),
                               READ_KEYS if index == 2 else EDIT_KEYS,
                               None if index == 2 else edited_record())
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
            if index == 1:
                frame_check(run / 'snap/nsb3_security_27.png',
                            '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231')
            else:
                frame_check(run / 'snap/nsb3_security_15.png',
                            'cd1d7a1ec0c2c9a68010741763172a5ff97b387537ffab997ac088d16b116d27')
                frame_check(run / 'snap/nsb3_security_16.png',
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
            prepare_reload(*args.runs, check_card)
    except (OSError, ValueError, KeyError, StopIteration) as exc:
        parser.exit(1, f'NSB-3 FDN edit: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own protected FDN edit/cold Details')


if __name__ == '__main__':
    main()
