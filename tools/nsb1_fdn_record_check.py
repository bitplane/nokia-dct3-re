"""Own NSB-1 protected FDN save, held-number cold view and rejection control."""
import argparse
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import check_cold_link, check_card_run, check_frame
from tools.nsb1_pin_roundtrip import check_contacts, ACCEPTED_RGB_SHA256, WRONG_RGB_SHA256
from tools.nsk3_phonebook_check import transmitted
from tools.sim_security_trace_check import statuses, padded_pin, ATTEMPTS_OFFSET
from tools.sim_fdn_profile import EXTENDED_LENGTH

RECORD = bytes.fromhex('41ffffffffffffffffffffffffffffffffff04812143f5ffffffffffffffffff')


def card_result(before, wrong=False):
    if (len(before) != EXTENDED_LENGTH or before[3570:3576] != b'FDN\x01\x01\x01'
            or before[3578:3898] != bytes([255]) * 320 or before[ATTEMPTS_OFFSET + 1] != 3):
        raise ValueError('fresh FDN-Off record and PIN2 retries required')
    after = bytearray(before)
    if wrong:
        after[ATTEMPTS_OFFSET + 1] = 2
    else:
        after[3578:3610] = RECORD
    return bytes(after)


def edited_card(before):
    if len(before) != EXTENDED_LENGTH or before[3575] != 1 or before[3578:3610] != RECORD:
        raise ValueError('saved FDN-Off A/12345 record required')
    after = bytearray(before)
    after[3600] = 0xf6
    return bytes(after)


def check_trace(trace, mode):
    if mode not in ('write', 'cold', 'wrong', 'edit', 'erase', 'cancel', 'empty'):
        raise ValueError('known FDN record mode required')
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    count = {'write': 23, 'cold': 11, 'wrong': 15, 'edit': 21, 'erase': 19, 'cancel': 19, 'empty': 10}[mode]
    contacts = re.findall(r'nsb1_fdn_contact: index=(\d+) down=([01]) ', trace)
    if contacts != [(str(i), down) for i in range(1, count + 1) for down in ('1', '0')]:
        raise ValueError('physical FDN record input differs')
    if trace.index('SIM status ins=20 sw=9000') > trace.index('nsb1_pin_contact: phase=phone index=1 down=1 '):
        raise ValueError('phone input preceded SIM authentication')
    if trace.index('nsb1_fdn_contact: index=1 down=1 ') < trace.index('nsb1_pin_contact: phase=phone index=6 down=0 '):
        raise ValueError('FDN input preceded phone submission')
    expected = [(0x9000, 3, 10)]
    if mode not in ('cold', 'empty'):
        expected.append((0x9804 if mode == 'wrong' else 0x9000, 3, 10))
    if statuses(trace, 0x20) != expected:
        raise ValueError('PIN1/PIN2 result differs')
    if any(statuses(trace, ins) for ins in (4, 0x44, 0x24, 0x26, 0x28, 0x2c)):
        raise ValueError('unrelated validity/credential operation')
    tx = transmitted(trace, 4096, 'nsb1_sim_tx')
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
        raise ValueError('FDN update result differs')
    if mode in ('write', 'edit', 'erase'):
        body = bytearray(RECORD)
        if mode == 'edit':
            body[22] = 0xf6
        elif mode == 'erase':
            body[:] = bytes([255]) * 32
        command = bytes.fromhex('a0dc010420') + body
        if tx.count(command) != 1 or tx.index(command) < tx.index(pin2):
            raise ValueError('protected FDN record payload/order differs')
    elif bytes.fromhex('a0dc') in tx:
        raise ValueError('cold read or rejection sent UPDATE RECORD')
    if mode == 'cold':
        held = re.findall(r'nsb1_fdn_contact: index=11 down=([01]) t=([0-9.]+)', trace)
        if len(held) != 2 or held[0][0] != '1' or held[1][0] != '0' or not 3.95 < float(held[1][1]) - float(held[0][1]) < 4.05:
            raise ValueError('number-view key was not held for four seconds')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'write', 'cold', 'wrong'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--edit', type=Path)
    parser.add_argument('--edit-cold', type=Path)
    args = parser.parse_args()
    try:
        before = (args.seed / 'nvram/nsb1r4t/sim_card').read_bytes()
        saved = card_result(before)
        for run, mode in ((args.write, 'write'), (args.cold, 'cold'), (args.wrong, 'wrong')):
            check_cold_link(run, args.write if mode == 'cold' else args.seed)
            code = json.loads((run / 'exit_status.json').read_text())['returncode']
            trace, card = check_card_run(run, code, {'write': 78, 'cold': 54, 'wrong': 60}[mode])
            if card != (card_result(before, True) if mode == 'wrong' else saved):
                raise ValueError('whole-card FDN record result differs')
            check_trace(trace, mode)
            check_frame(run / 'snap/nsb1_pin_reload_4.png', ACCEPTED_RGB_SHA256)
        check_frame(args.write / 'snap/nsb1_fdn_71.png', SAVED_FRAME)
        check_frame(args.cold / 'snap/nsb1_fdn_49.png', NUMBER_FRAME)
        check_frame(args.cold / 'snap/nsb1_fdn_51.png', NAME_FRAME)
        check_frame(args.wrong / 'snap/nsb1_fdn_55.png', WRONG_RGB_SHA256)
        if bool(args.edit) != bool(args.edit_cold):
            raise ValueError('edit/cold controls must be paired')
        if args.edit:
            edited = edited_card(saved)
            previous = args.cold
            for run, cold in ((args.edit, False), (args.edit_cold, True)):
                check_cold_link(run, previous)
                code = json.loads((run / 'exit_status.json').read_text())['returncode']
                trace, card = check_card_run(run, code, 54 if cold else 74)
                if card != edited:
                    raise ValueError('in-place edit changed unrelated card bytes')
                check_trace(trace, 'cold' if cold else 'edit')
                previous = run
            check_frame(args.edit / 'snap/nsb1_fdn_67.png', SAVED_FRAME)
            check_frame(args.edit_cold / 'snap/nsb1_fdn_49.png', EDITED_NUMBER_FRAME)
            check_frame(args.edit_cold / 'snap/nsb1_fdn_51.png', NAME_FRAME)
    except (OSError, ValueError, KeyError) as error:
        print(f'NSB-1 FDN record failed: {error}', file=sys.stderr)
        return 1
    print('NSB-1 physical PIN2-protected FDN save, cold12345 view and rejected-write preservation PASS')
    if args.edit:
        print('NSB-1 physical in-place FDN edit and unchanged cold12346 view PASS')
    return 0


SAVED_FRAME = '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231'
NUMBER_FRAME = 'f7b1397133d10be7ddad9597bf4acdc99928be48d1d9e28e21d432a48a09679e'
NAME_FRAME = 'cb43bd52bd90957fce71d365b0b4c2ed69079f6f6470aa702058ab4824006db5'
EDITED_NUMBER_FRAME = 'ad7b28c5c7bb2ce2093492172567c7d2b60c8961e2db5dad0f966da0516ae759'

if __name__ == '__main__':
    raise SystemExit(main())
