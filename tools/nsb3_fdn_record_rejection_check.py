"""Own NSB-3 failed protected FDN Add preserves existing records across cold read."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_fdn_record_check import NUMBERS, check_card, check_trace as check_read, check_record_environment
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import padded_pin, statuses
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME

KEYS = NUMBERS + ('Scroll Down', 'Left Softkey / Menu') + tuple(
    'Keypad ' + digit for digit in '5679') + ('Left Softkey / Menu',)


def check_rejection(text):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts protected-write rejection')
    boundary = text.find('nsb3_security_key:')
    if boundary < 0:
        raise ValueError('physical application boundary missing')
    check_pin(text[:boundary], True)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in KEYS for state in ('1', '0')]:
        raise ValueError('physical rejected FDN Add contacts differ')
    commands = wire(text)
    verify = [row[:5] for row in commands if row[0] == 0x20]
    if verify != [(0x20, 0, 1, 8, padded_pin('1234')),
                  (0x20, 0, 2, 8, padded_pin('5679'))]:
        raise ValueError('startup/failed PIN2 payloads differ')
    if statuses(text, 0x20) != [(0x9000, 3, 10), (0x9804, 3, 10)]:
        raise ValueError('startup/failed PIN2 statuses differ')
    check_record_environment(text, commands)
    if any(row[0] == 0xdc for row in commands) or statuses(text, 0xdc):
        raise ValueError('denied FDN Add must not write any record')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires retained-contact source/rejected/cold-read')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        check_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(), 3 if index == 0 else 2)
        if not index:
            continue
        check_link(runs[index - 1], run)
        text = (run / 'error.log').read_text()
        if index == 1:
            check_rejection(text)
            frame_check(run / 'snap/nsb3_security_20.png',
                        'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e')
        else:
            check_read(text, True)
            frame_check(run / 'snap/nsb3_security_15.png',
                        'cd1d7a1ec0c2c9a68010741763172a5ff97b387537ffab997ac088d16b116d27')
            frame_check(run / 'snap/nsb3_security_16.png',
                        'c107145ed4247dc6f298f7811d4c0400481827093abccb703d7f559825910813')
        frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)


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
                raise ValueError('requires rejected source and cold destination')
            prepare_reload(*args.runs, lambda card: check_card(card, 2))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 rejected FDN record: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' rejected protected FDN Add/cold preservation')


if __name__ == '__main__':
    main()
