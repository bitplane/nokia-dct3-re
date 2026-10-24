"""Qualify own NSB-3 Names/Options and SIM-dependent capacity observation."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_phonebook_check import check_card, check_security_menu, prepare_reload, wire
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check

KEYS = ('Right Softkey / C', 'Right Softkey / C') + ('Scroll Down',) * 3 + (
    'Left Softkey / Menu', 'Scroll Down', 'Scroll Down', 'Scroll Down', 'Left Softkey / Menu')


def check_trace(text):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts Names/Options observation')
    check_security_menu(text)
    contacts = re.findall(r'nsb3_phonebook_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in KEYS for state in ('1', '0')]:
        raise ValueError('physical Names/Options traversal differs')
    commands = wire(text)
    if any(row[0] in (4, 0x44, 0xd6, 0xdc, 0x20, 0x24, 0x26, 0x28, 0x2c, 0x32, 0xc2) for row in commands):
        raise ValueError('capacity observation must not mutate/authenticate the card')
    if not any(row[:4] == (0xb2, 1, 4, 32) and row[5] == 0x6f3a for row in commands):
        raise ValueError('ADN read control missing')


def check_run(source, run):
    before = (source / 'nvram/nsb3r4t/sim_card').read_bytes()
    check_card(before)
    check_link(source, run)
    for path in (source, run):
        if hashlib.sha256((path / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
    if (run / 'nvram/nsb3r4t/sim_card').read_bytes() != before:
        raise ValueError('Options observation changed complete SIM')
    check_trace((run / 'error.log').read_text())
    for index in (6, 8):
        frame_check(run / f'snap/nsb3_phonebook_{index:02d}.png',
                    '4ac28208c96be2a5e5c0ceffcbacdd75fa4c887935bef7d95be0c2cac26971b6')
    for index in (7, 9):
        frame_check(run / f'snap/nsb3_phonebook_{index:02d}.png',
                    'fcfb2f38c8ab7283babef610e0a9a727564f543fe104041c05d6e815ba95997c')
    frame_check(run / 'snap/nsb3_phonebook_10.png',
                'cebb86d763c4542af1986e5afccf705e79ecf716942f53af6dc053eb0db62518')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare', 'check'))
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'prepare':
            prepare_reload(args.source, args.run)
        else:
            check_run(args.source, args.run)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 Names/Options: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own Names/Options capacity boundary; no Phone-store assumption')


if __name__ == '__main__':
    main()
