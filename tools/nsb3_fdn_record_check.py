"""Own NSB-3 PIN2-protected FDN record save and independently cold Details."""
import argparse
import hashlib
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_fdn_check import PREFIX, expected_card as empty_card, check_card as check_empty
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import prepare_reload, wire, expected_record
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import padded_pin, statuses, ATTEMPTS_OFFSET
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME

NUMBERS = PREFIX + ('Left Softkey / Menu', 'Scroll Down', 'Scroll Down', 'Left Softkey / Menu')
WRITE_KEYS = NUMBERS + ('Scroll Down', 'Left Softkey / Menu') + tuple(
    'Keypad ' + digit for digit in '5678') + ('Left Softkey / Menu', 'Keypad 2',
    'Left Softkey / Menu') + tuple('Keypad ' + digit for digit in '12345') + ('Left Softkey / Menu',)
READ_KEYS = NUMBERS + ('Left Softkey / Menu',) * 3
FDN_OFFSET = 3578


def expected_card(pin2_attempts=3):
    if pin2_attempts not in range(4):
        raise ValueError('invalid PIN2 retry expectation')
    card = bytearray(empty_card(True))
    card[FDN_OFFSET:FDN_OFFSET + 32] = expected_record()
    card[ATTEMPTS_OFFSET + 1] = pin2_attempts
    return bytes(card)


def check_card(card, pin2_attempts=3):
    if card != expected_card(pin2_attempts):
        raise ValueError('FDN A/12345 card differs beyond the qualified record')


def check_trace(text, readback=False):
    check_record_trace(text, READ_KEYS if readback else WRITE_KEYS,
                       None if readback else expected_record())


def check_record_trace(text, keys, record=None, authenticate_read=False):
    readback = record is None
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts FDN record qualification')
    verifies = ((0x20, 0, 2, 8, padded_pin('5678')),) if not readback or authenticate_read else ()
    check_pin(text, True, additional_verifies=verifies)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical FDN record contacts differ')
    commands = wire(text)
    updates = [row for row in commands if row[0] == 0xdc]
    if updates != ([] if readback else [(0xdc, 1, 4, 32, record, 0x6f3b)]):
        raise ValueError('requires one exact EF_FDN update then read-only cold retrieval')
    if statuses(text, 0xdc) != ([] if readback else [(0x9000, 3, 10)]):
        raise ValueError('FDN update status differs')
    if not readback:
        verify = next(i for i, row in enumerate(commands) if row[:3] == (0x20, 0, 2))
        update = next(i for i, row in enumerate(commands) if row[0] == 0xdc)
        if update <= verify:
            raise ValueError('FDN update precedes PIN2 authentication')
    check_record_environment(text, commands)


def check_record_environment(text, commands):
    rehab = [row for row in commands if row[0] == 0x44]
    if [row[5] for row in rehab] != [0x6f7e, 0x6f07] or any(row[1:5] != (0, 0, 0, b'') for row in rehab):
        raise ValueError('active-card bootstrap identity rehabilitation differs')
    if statuses(text, 0x44) != [(0x9000, 3, 10)] * 2:
        raise ValueError('identity rehabilitation status differs')
    if not any(row[:4] == (0xb2, 1, 4, 32) and row[5] == 0x6f3b for row in commands):
        raise ValueError('FDN record-one read control missing')
    if any(row[0] in (4, 0xd6, 0x24, 0x26, 0x28, 0x2c, 0x32, 0xc2) for row in commands):
        raise ValueError('unrelated validity/credential/file/Toolkit mutation')


def check_chain(runs):
    if len(runs) != 3:
        raise ValueError('requires enabled-empty source/save/cold-read')
    check_empty((runs[0] / 'nvram/nsb3r4t/sim_card').read_bytes(), True)
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if index:
            check_link(runs[index - 1], run)
            check_card((run / 'nvram/nsb3r4t/sim_card').read_bytes())
            check_trace((run / 'error.log').read_text(), index == 2)
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
            if index == 1:
                frame_check(run / 'snap/nsb3_security_28.png',
                            '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231')
            else:
                frame_check(run / 'snap/nsb3_security_15.png',
                            'cd1d7a1ec0c2c9a68010741763172a5ff97b387537ffab997ac088d16b116d27')
                frame_check(run / 'snap/nsb3_security_16.png',
                            'c107145ed4247dc6f298f7811d4c0400481827093abccb703d7f559825910813')


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
                raise ValueError('requires saved source and cold destination')
            prepare_reload(*args.runs, check_card)
    except (OSError, ValueError, KeyError, StopIteration) as exc:
        parser.exit(1, f'NSB-3 FDN record: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own protected FDN save/cold Details; radio unqualified')


if __name__ == '__main__':
    main()
