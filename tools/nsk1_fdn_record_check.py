"""Own NSK-1 protected FDN save/edit, cold held-number read and rejection."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_fdn_check import MENU, check_input, check_trace as check_validity, OFF_FRAME
from tools.nsk1_pin2_change_check import check_cold_link
from tools.nsk1_security_compare import check_phase
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsk1_pin_roundtrip import CODE_ERROR
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, statuses, padded_pin

# Comparison bytes for the physically entered test contact, never a seeded file.
RECORD = bytes.fromhex('41ffffffffffffffffffffffffffffffffff04812143f5ffffffffffffffffff')
WRITE_KEYS = MENU + ['down','menu','down','menu','five','six','seven','eight','menu',
                     'two','menu','one','two','three','four','five','menu']
READ_KEYS = MENU + ['down','menu','menu','menu','hash']
EDIT_KEYS = READ_KEYS[:-1] + ['menu','menu','five','six','seven','eight','menu','menu','clear','six','menu']
ERASE_KEYS = READ_KEYS[:-1] + ['menu','down','menu','five','six','seven','eight','menu','menu']
SAVED_LIST = 'cde17cf68445036ced7db33a3a93c978d5068388e8031c6dff9df305ba4ba970'
NUMBER = '55bdfda888bb09881233e5b91ad607532185f77734260a1c5b813553ccbad9bd'
EDITED_NUMBER = '72c38308118b60921e06f8b9fcc1feee03e2fba233a053c73cf66a818e417e4b'


def expected_card(before, wrong=False):
    if (len(before) != 3898 or before[3570:3576] != b'FDN\x01\x01\x01'
            or before[3578:3898] != b'\xff'*320 or before[ATTEMPTS_OFFSET+1] != 3):
        raise ValueError('empty FDN-Off card and three PIN2 retries required')
    after = bytearray(before)
    if wrong:
        after[ATTEMPTS_OFFSET+1] = 2
    else:
        after[3578:3610] = RECORD
    return bytes(after)


def edited_card(before):
    if len(before) != 3898 or before[3575] != 1 or before[3578:3610] != RECORD:
        raise ValueError('saved FDN-Off A/12345 record required')
    after = bytearray(before)
    after[3600] = 0xf6
    return bytes(after)


def erased_card(before):
    record = bytearray(RECORD)
    record[22] = 0xf6
    if len(before) != 3898 or before[3575] != 1 or before[3578:3610] != record:
        raise ValueError('edited FDN-Off A/12346 record required')
    after = bytearray(before)
    after[3578:3610] = b'\xff'*32
    return bytes(after)


def check_trace(trace, mode):
    if mode not in ('write', 'cold', 'wrong', 'edit', 'erase', 'cancel', 'empty'):
        raise ValueError('known record mode required')
    keys = READ_KEYS if mode == 'cold' else WRITE_KEYS[:]
    if mode == 'edit':
        keys = EDIT_KEYS
    if mode in ('erase', 'cancel'):
        keys = ERASE_KEYS[:]
        if mode == 'cancel':
            keys[-1] = 'clear'
    if mode == 'empty':
        keys = READ_KEYS[:-1]
    if mode == 'wrong':
        keys = keys[:19]
        keys[17] = 'five'
    check_input(trace, keys)
    expected = [(0x9000, 3, 10)]
    if mode not in ('cold', 'empty'):
        expected.append((0x9804 if mode == 'wrong' else 0x9000, 3, 10))
    if statuses(trace, 0x20) != expected:
        raise ValueError('PIN1/PIN2 verification differs')
    if any(statuses(trace, ins) for ins in (4, 0x44, 0x24, 0x26, 0x28, 0x2c, 0xd6)):
        raise ValueError('unrelated validity/credential/file operation')
    if statuses(trace, 0xdc) != ([(0x9000, 3, 10)] if mode in ('write', 'edit', 'erase') else []):
        raise ValueError('record update status differs')
    tx = transmitted(trace, 4096, 'nsk1_sim_tx')
    if len(tx) >= 4096 or tx.count(bytes.fromhex('a020000108') + padded_pin('1234')) != 1:
        raise ValueError('incomplete SIM observation or startup PIN payload')
    if bytes.fromhex('a0a40000026f3b') not in tx:
        raise ValueError('FDN selection absent')
    pin2 = bytes.fromhex('a020000208') + padded_pin('5675' if mode == 'wrong' else '5678')
    if mode not in ('cold', 'empty') and tx.count(pin2) != 1:
        raise ValueError('PIN2 payload differs')
    updates = re.findall(r'update fid=([0-9a-f]+) record=(\d+) length=(\d+)', trace)
    if updates != ([('6f3b', '1', '32')] if mode in ('write', 'edit', 'erase') else []):
        raise ValueError('unexpected file update')
    if mode in ('write', 'edit', 'erase'):
        record = bytearray(RECORD)
        if mode == 'edit':
            record[22] = 0xf6
        if mode == 'erase':
            record[:] = b'\xff'*32
        command = bytes.fromhex('a0dc010420') + record
        if tx.count(command) != 1 or tx.index(command) < tx.index(pin2):
            raise ValueError('protected record payload/order differs')
    elif bytes.fromhex('a0dc') in tx:
        raise ValueError('non-writing operation submitted UPDATE RECORD')
    if mode == 'cold':
        if bytes.fromhex('a0b2010420') not in tx:
            raise ValueError('cold record read absent')
        held = re.findall(r'nsk1_fdn_contact: index=15 key=hash down=([01]) t=([0-9.]+)', trace)
        if len(held) != 2 or held[0][0] != '1' or held[1][0] != '0' or not 3.95 < float(held[1][1])-float(held[0][1]) < 4.05:
            raise ValueError('number-view contact not held four seconds')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'write', 'cold', 'wrong'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--edit', nargs=2, type=Path, metavar=('EDIT', 'COLD'))
    args = parser.parse_args()
    try:
        _, seed_trace = check_phase(args.seed, (args.seed / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
        check_validity(seed_trace, 3)
        frame_check(args.seed / 'snap/nsk1_fdn_final.png', OFF_FRAME)
        before = (args.seed / 'nvram/nsk1r4t/sim_card').read_bytes()
        saved = expected_card(before)
        for run, mode in ((args.write, 'write'), (args.cold, 'cold'), (args.wrong, 'wrong')):
            check_cold_link(run, args.write if mode == 'cold' else args.seed)
            _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
            card = (run / 'nvram/nsk1r4t/sim_card').read_bytes()
            if card != (expected_card(before, True) if mode == 'wrong' else saved):
                raise ValueError('whole-card result differs')
            check_trace(trace, mode)
            frame_check(run / 'snap/nsk1_fdn_final.png', {'write': SAVED_LIST, 'cold': NUMBER, 'wrong': CODE_ERROR}[mode])
        if args.edit:
            edit, cold = args.edit
            for run, previous, mode in ((edit, args.cold, 'edit'), (cold, edit, 'cold')):
                check_cold_link(run, previous)
                _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
                if (run / 'nvram/nsk1r4t/sim_card').read_bytes() != edited_card(saved):
                    raise ValueError('in-place edit/cold card differs')
                check_trace(trace, mode)
                frame_check(run / 'snap/nsk1_fdn_final.png', SAVED_LIST if mode == 'edit' else EDITED_NUMBER)
    except (ValueError, OSError, KeyError) as error:
        print(f'NSK-1 protected FDN record failed: {error}', file=sys.stderr)
        return 1
    print('NSK-1 protected FDN save, cold read and rejected-write preservation PASS')
    if args.edit:
        print('NSK-1 in-place FDN edit and cold 12346 retrieval PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
