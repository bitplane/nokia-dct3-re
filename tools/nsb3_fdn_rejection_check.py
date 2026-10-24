"""Physical NSB-3 rejected FDN activation and exact cold PIN2 retry recovery."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_fdn_check import PREFIX, expected_card, check_card, check_fixture, check_trace as check_activation
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, padded_pin, statuses
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME


def rejected_card():
    card = bytearray(expected_card())
    card[ATTEMPTS_OFFSET + 1] = 2
    return bytes(card)


def check_rejected_card(card):
    if card != rejected_card():
        raise ValueError('rejected FDN activation changed more than PIN2 retries')


def check_rejection(text):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts FDN rejection qualification')
    boundary = text.find('nsb3_security_key:')
    if boundary < 0:
        raise ValueError('application input boundary missing')
    # Startup owns its successful PIN1 VERIFY; the full stream below separately
    # requires the failed application PIN2 VERIFY without discarding that failure.
    check_pin(text[:boundary], True)
    keys = PREFIX + ('Left Softkey / Menu', 'Scroll Up', 'Left Softkey / Menu') + tuple(
        'Keypad ' + digit for digit in '5679') + ('Left Softkey / Menu',)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical rejected FDN activation contacts differ')
    commands = wire(text)
    verify = [row[:5] for row in commands if row[0] == 0x20]
    if verify != [(0x20, 0, 1, 8, padded_pin('1234')),
                  (0x20, 0, 2, 8, padded_pin('5679'))]:
        raise ValueError('startup/failed PIN2 verification payloads differ')
    if statuses(text, 0x20) != [(0x9000, 3, 10), (0x9804, 3, 10)]:
        raise ValueError('startup/failed PIN2 statuses differ')
    if any(row[0] in (4, 0x44, 0xdc, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32, 0xc2) for row in commands):
        raise ValueError('denied activation must not change validity, credentials or files')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires base/rejected/cold-retry')
    check_fixture(runs[0], runs[1])
    for index, run in enumerate(runs[1:], 1):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        text = (run / 'error.log').read_text()
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        if index == 1:
            check_rejected_card(card)
            check_rejection(text)
        else:
            check_link(runs[1], run)
            check_card(card, True)
            check_activation(text, 0)
        frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
        frame_check(run / 'snap/nsb3_security_17.png',
                    'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
                    if index == 1 else
                    'aed44705c9a6e9f8d18ba34092b4775f8ed74168b9c60f5ed4f1a9bff3ebbd72')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-retry', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires rejected source and cold destination')
            prepare_reload(*args.runs, check_rejected_card)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 rejected FDN activation: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' rejected FDN activation/cold PIN2 retry')


if __name__ == '__main__':
    main()
