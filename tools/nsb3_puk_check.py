"""Check own NSB-3 physical PIN exhaustion, PUK recovery and cold replacement PIN."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_pin_check import check_card, check_source, check_trace as check_pin, PIN_PROMPT, WRONG_FRAME, PHONE_PROMPT, MENU_FRAME
from tools.nsb3_menu_check import check_trace as check_phone
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.sim_security_trace_check import statuses, padded_pin
from tools.nsk1_phonebook_roundtrip import frame_check


def check_recovery(text, wrong=False, remaining=9):
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupts PUK qualification')
    expected = []
    puk = '12345679' if wrong else '12345678'
    for stage, digits in (('puk', puk), ('new', '2468'), ('repeat', '2468')):
        keys = ['Keypad ' + digit for digit in digits] + ['Left Softkey / Menu']
        expected.extend((stage, key, down) for key in keys for down in ('1', '0'))
    contacts = re.findall(r'nsb3_puk_key: stage=(\w+) key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != expected:
        raise ValueError('physical PUK/new-PIN/repeat contacts differ')
    commands = wire(text)
    credential = [row[:5] for row in commands if row[0] in (0x20, 0x24, 0x26, 0x28, 0x2c)]
    if credential != [(0x2c, 0, 0, 16, puk.encode() + padded_pin('2468'))]:
        raise ValueError('requires one physical PUK1/replacement UNBLOCK transaction')
    expected_status = (0x9804 if remaining else 0x9840, 0, remaining) if wrong else (0x9000, 3, 10)
    if statuses(text, 0x2c) != [expected_status] or statuses(text, 0x20):
        raise ValueError('PUK recovery status differs')
    if wrong:
        if 'nsb3_phonecode_key:' in text:
            raise ValueError('wrong PUK must not enter phone authentication')
    else:
        check_phone(text)


def check_link(source, target):
    provenance = json.loads((target / 'cold_source.json').read_text())
    if provenance.get('copied_guest_ram_or_save_state') is not False:
        raise ValueError('cold source provenance missing')
    for name in ('flash', 'eeprom', 'sim_card'):
        if provenance['storage_sha256'][name] != hashlib.sha256((source / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest():
            raise ValueError('cold own-storage source differs')
    if (target / 'initial_sim_card.bin').read_bytes() != (source / 'nvram/nsb3r4t/sim_card').read_bytes():
        raise ValueError('exact persistent retry/PIN state was not seeded')


def check_chain(runs):
    if len(runs) != 5:
        raise ValueError('requires three wrong-PIN runs, recovery and cold new-PIN run')
    check_source(runs[0])
    for index, run in enumerate(runs):
        text = (run / 'error.log').read_text()
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        if index < 3:
            check_pin(text, False, 3 - index)
            check_card(card, 2 - index)
            frame_check(run / 'snap/nsb3_pin_prompt.png', PIN_PROMPT)
            frame_check(run / 'snap/nsb3_pin_result.png', WRONG_FRAME if index < 2 else
                        '55b898bce51fda88f082ee5a0fc9e8b35a7a9a5cfa0f6799a2b93eba5373a3f8')
        elif index == 3:
            check_recovery(text)
            check_card(card, 3, '2468')
            for name, digest in (
                    ('nsb3_puk_prompt.png', 'f33cc6271f7868cfa340d6cc0643dbb6a1063abbafe8624b19fed6dc7e480e2b'),
                    ('nsb3_puk_stage_1.png', 'f9f25f5c58974422205e4731d4d075aa4d17235f823f66efc15f39b528634fae'),
                    ('nsb3_puk_stage_2.png', 'ed561811d9b6f6e2dc1839453a4bde8e4a055a96c9a05e85de180d90c3937760'),
                    ('nsb3_puk_stage_3.png', PHONE_PROMPT), ('nsb3_phonecode_menu.png', MENU_FRAME)):
                frame_check(run / 'snap' / name, digest)
        else:
            check_pin(text, True, pin='2468')
            check_card(card, 3, '2468')
            frame_check(run / 'snap/nsb3_pin_prompt.png', PIN_PROMPT)
            frame_check(run / 'snap/nsb3_pin_result.png', PHONE_PROMPT)
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if index:
            check_link(runs[index - 1], run)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-unblock', 'prepare-new-pin', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('cold preparation requires source and destination')
            retries, pin = (0, '1234') if args.operation == 'prepare-unblock' else (3, '2468')
            prepare_reload(*args.runs, lambda card: check_card(card, retries, pin))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PUK recovery: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own PIN/PUK contract; network unproved')


if __name__ == '__main__':
    main()
