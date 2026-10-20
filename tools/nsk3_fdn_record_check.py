"""Own NSK-3 physical FDN save/cold read with wrong-PIN2 preservation."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk3_static_check import inspect
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses, padded_pin, ATTEMPTS_OFFSET
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.nsk1_phonebook_roundtrip import frame_check

PREFIX = (0x19, 4, 5, 3, 0x19, 0x18, 0x19, 0x18, 0x19)
READ_KEYS = (0x19, 4, 5, 3, 0x19, 0x18, 0x19, 0x19, 0x19, 0x19)
RECORD = bytes.fromhex('41ffffffffffffffffffffffffffffffffff04812143f5ffffffffffffffffff')


def expected_card(before, wrong=False):
    if (len(before) != EXTENDED_LENGTH or before[3570:3576] != b'FDN\x01\x01\x01'
            or before[3578:3898] != bytes([255]) * 320 or before[ATTEMPTS_OFFSET + 1] != 3):
        raise ValueError('fresh FDN-Off card and PIN2 retries required')
    after = bytearray(before)
    if wrong:
        after[ATTEMPTS_OFFSET + 1] = 2
    else:
        after[3578:3610] = RECORD
    return bytes(after)


def check_trace(trace, mode):
    if mode not in ('write', 'cold', 'wrong', 'edit', 'erase', 'cancel', 'empty'):
        raise ValueError('unknown FDN record phase')
    keys = READ_KEYS if mode == 'cold' else PREFIX + (5, 6, 7, 5 if mode == 'wrong' else 8, 0x19)
    if mode == 'write':
        keys += (2, 0x19, 0x19, 1, 2, 3, 4, 5, 0x19)
    elif mode == 'edit':
        keys = READ_KEYS + (0x19, 5, 6, 7, 8, 0x19, 0x19, 0x19, 0x19, 0x1a, 6, 0x19)
    elif mode in ('erase', 'cancel'):
        keys = READ_KEYS + (0x19, 5, 6, 7, 8, 0x19, 0x19, 0x18, 0x19,
                            0x1a if mode == 'cancel' else 0x19)
    elif mode == 'empty':
        keys = READ_KEYS[:9]
    check_phone(trace, True, keys)
    contacts = re.findall(r'nsk3_pin_contact: correct=1 key=([0-9a-f]+) down=([01]) ', trace)
    if contacts != [(f'{key:02x}', down) for key in (1, 2, 3, 4, 0x19) for down in ('1', '0')]:
        raise ValueError('physical startup PIN differs')
    if trace.index('SIM status ins=20 sw=9000') > trace.index('nsk3_security_submit:'):
        raise ValueError('phone authentication preceded SIM authentication')
    application = list(re.finditer(r'nsk3_security_contact: key=[0-9a-f]+ down=1 ', trace))
    if application[6].start() < trace.index('nsk3_security_accepted:'):
        raise ValueError('application input preceded phone acceptance')
    results = [(0x9000, 3, 10)]
    if mode not in ('cold', 'empty'):
        results.append((0x9804 if mode == 'wrong' else 0x9000, 3, 10))
    if statuses(trace, 0x20) != results:
        raise ValueError('PIN1/PIN2 results differ')
    if any(statuses(trace, ins) for ins in (4, 0x44, 0x24, 0x26, 0x28, 0x2c)):
        raise ValueError('unexpected validity/credential mutation')
    tx = transmitted(trace, 4096, 'nsk3_sim_tx')
    if len(tx) >= 4096:
        raise ValueError('SIM observation cap reached')
    if tx.count(bytes.fromhex('a020000108') + padded_pin('1234')) != 1:
        raise ValueError('startup PIN payload differs')
    if bytes.fromhex('a0a40000026f3b') not in tx:
        raise ValueError('FDN selection absent')
    if mode not in ('cold', 'empty'):
        pin2 = bytes.fromhex('a020000208') + padded_pin('5675' if mode == 'wrong' else '5678')
        if tx.count(pin2) != 1:
            raise ValueError('PIN2 payload differs')
    if statuses(trace, 0xdc) != ([(0x9000, 3, 10)] if mode in ('write', 'edit', 'erase') else []):
        raise ValueError('record update result differs')
    command = bytes.fromhex('a0dc010420') + RECORD
    if mode == 'edit':
        body = bytearray(RECORD)
        body[22] = 0xf6
        command = bytes.fromhex('a0dc010420') + body
    elif mode == 'erase':
        command = bytes.fromhex('a0dc010420') + bytes([255]) * 32
    if mode in ('write', 'edit', 'erase'):
        if tx.count(command) != 1 or tx.index(command) < tx.index(pin2):
            raise ValueError('protected record update differs')
    elif bytes.fromhex('a0dc') in tx:
        raise ValueError('read/rejection sent an UPDATE RECORD')
    check_upload_handoffs(trace, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'write', 'cold', 'wrong'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--edit', type=Path)
    parser.add_argument('--edit-cold', type=Path)
    args = parser.parse_args()
    try:
        _, flash = inspect(args.source, 'boot4')
        before = (args.write / 'initial_sim_card.bin').read_bytes()
        saved = expected_card(before)
        for run, mode in ((args.write, 'write'), (args.cold, 'cold'), (args.wrong, 'wrong')):
            storage = run / 'nvram/nsk3r4t'
            if (run / 'initial_sim_card.bin').read_bytes() != (saved if mode == 'cold' else before):
                raise ValueError('card input differs')
            if (storage / 'sim_card').read_bytes() != (expected_card(before, True) if mode == 'wrong' else saved):
                raise ValueError('whole-card result differs')
            if (storage / 'flash').read_bytes() != flash:
                raise ValueError('own flash changed')
            check_trace((run / 'error.log').read_text(), mode)
        if (args.cold / 'initial_eeprom.bin').read_bytes() != (args.write / 'nvram/nsk3r4t/eeprom').read_bytes():
            raise ValueError('cold EEPROM link differs')
        if (args.wrong / 'initial_eeprom.bin').read_bytes() != (args.write / 'initial_eeprom.bin').read_bytes():
            raise ValueError('negative EEPROM input differs')
        frame_check(args.cold / 'snap/nsk3_fdn_read_29.png',
                    'a089a5b358dd43b2f090d972dadb617c935180f95923efa43f99e9e330c055d0')
        if bool(args.edit) != bool(args.edit_cold):
            raise ValueError('edit and edit-cold must be paired')
        if args.edit:
            edited = bytearray(saved)
            edited[3600] = 0xf6
            previous = args.cold
            for run, mode in ((args.edit, 'edit'), (args.edit_cold, 'cold')):
                storage = run / 'nvram/nsk3r4t'
                if (run / 'initial_sim_card.bin').read_bytes() != (saved if mode == 'edit' else edited):
                    raise ValueError('edit card input differs')
                if (storage / 'sim_card').read_bytes() != edited:
                    raise ValueError('edit changed unrelated card bytes')
                if (run / 'initial_eeprom.bin').read_bytes() != (previous / 'nvram/nsk3r4t/eeprom').read_bytes():
                    raise ValueError('edit EEPROM cold link differs')
                if (storage / 'flash').read_bytes() != flash:
                    raise ValueError('edit changed own flash')
                check_trace((run / 'error.log').read_text(), mode)
                previous = run
            frame_check(args.edit_cold / 'snap/nsk3_fdn_read_29.png',
                        'a05b707808f144e8ba04fada27a7ed5557521a51c73497beb2ef0756f053836e')
    except (OSError, ValueError) as error:
        print(f'NSK-3 FDN record failed: {error}', file=sys.stderr)
        return 1
    print('NSK-3 physical protected FDN save/cold read and wrong-PIN2 preservation PASS')
    if args.edit:
        print('NSK-3 physical FDN edit and unchanged cold12346 display PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
