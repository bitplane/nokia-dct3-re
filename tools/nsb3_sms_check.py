"""Qualify physical NSB-3 offline SMS draft save/cold read, not delivery."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_menu_check import check_trace as check_security
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_sms_draft import check_card, check_deleted_card, deleted_card, DRAFT

SAVE_KEYS = ('Left Softkey / Menu', 'Left Softkey / Menu', 'Scroll Down',
             'Scroll Down', 'Left Softkey / Menu', 'Keypad 2', 'Left Softkey / Menu',
             'Scroll Down', 'Scroll Down', 'Left Softkey / Menu')
READ_KEYS = ('Left Softkey / Menu', 'Left Softkey / Menu', 'Scroll Down',
             'Left Softkey / Menu', 'Left Softkey / Menu')
DELETE_KEYS = READ_KEYS + ('Left Softkey / Menu',) * 3


def check_trace(text, phase='save'):
    check_security(text)
    keys = {'save': SAVE_KEYS, 'read': READ_KEYS, 'delete': DELETE_KEYS, 'empty': READ_KEYS[:4]}.get(phase)
    if keys is None:
        raise ValueError('unknown SMS phase')
    rows = re.findall(r'nsb3_sms_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    expected = [(key, state) for key in keys for state in ('1', '0')]
    if rows != expected:
        raise ValueError('physical SMS contacts differ')
    commands = wire(text)
    updates = [row for row in commands if row[0] == 0xdc]
    if phase in ('read', 'empty'):
        if updates or not any(row[:4] == (0xb2, 1, 4, 176) and row[5] == 0x6f3c for row in commands):
            raise ValueError('cold SMS read must read record one without UPDATE RECORD')
    else:
        record = b'\x00' + DRAFT[1:] if phase == 'delete' else DRAFT
        if updates != [(0xdc, 1, 4, 176, record, 0x6f3c)]:
            raise ValueError('requires exactly one qualified SMS UPDATE RECORD')


def check_pair(save, cold):
    cards = []
    for run, readback in ((save, False), (cold, True)):
        check_trace((run / 'error.log').read_text(), 'read' if readback else 'save')
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        check_card(card)
        cards.append(card)
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
    if cards[0] != cards[1]:
        raise ValueError('cold retrieval changed persisted card')
    provenance = json.loads((cold / 'cold_source.json').read_text())
    if provenance.get('copied_guest_ram_or_save_state') is not False:
        raise ValueError('cold provenance missing')
    for name in ('flash', 'eeprom', 'sim_card'):
        if provenance['storage_sha256'][name] != hashlib.sha256((save / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest():
            raise ValueError('cold source own storage differs')
    frame_check(save / 'snap/nsb3_sms_06.png', '5e1c7514b1bbee44ee0acc3563cf74595023ed6cacc203147c0b4ed78c47b5f0')
    frame_check(save / 'snap/nsb3_sms_10.png', 'd4562fa5c9fbc8bb0180f9361ffebbecf39ad0bcda7708ab15e45b196b4144eb')
    frame_check(cold / 'snap/nsb3_sms_05.png', '126383b4fff9fc2d2e776b939ad18b3ae6d7194e9370a41a38d7313c7eca79cb')


def check_deletion(save, deleted, empty):
    saved = (save / 'nvram/nsb3r4t/sim_card').read_bytes()
    expected = deleted_card(saved)
    for run, phase in ((deleted, 'delete'), (empty, 'empty')):
        check_trace((run / 'error.log').read_text(), phase)
        if (run / 'nvram/nsb3r4t/sim_card').read_bytes() != expected:
            raise ValueError('delete/cold-empty changed more than SMS status byte')
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed during deletion lifecycle')
    for run, source in ((deleted, save), (empty, deleted)):
        provenance = json.loads((run / 'cold_source.json').read_text())
        if provenance.get('copied_guest_ram_or_save_state') is not False:
            raise ValueError('delete/cold-empty provenance missing')
        for name in ('flash', 'eeprom', 'sim_card'):
            if provenance['storage_sha256'][name] != hashlib.sha256((source / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest():
                raise ValueError('delete/cold-empty source storage differs')
    frame_check(deleted / 'snap/nsb3_sms_07.png', '0efe3c5798b90f0653bb227f9cce5afca2af17cdca71701c6b6b1e31b0aeb129')
    frame_check(deleted / 'snap/nsb3_sms_08.png', 'a60c4b9f39973847843cb184ff7ea1e5b2ffc5913d09e825df22c7cabaaf4173')
    frame_check(empty / 'snap/nsb3_sms_04.png', 'cdff157a2a10f5c1528e9085b1981401a9714ae37cded35e3eddd43fd8a83fb0')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-reload', 'prepare-empty', 'check'))
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('--deleted', type=Path)
    parser.add_argument('--empty', type=Path)
    args = parser.parse_args()
    try:
        if args.operation in ('prepare-reload', 'prepare-empty'):
            prepare_reload(args.source, args.target,
                           check_deleted_card if args.operation == 'prepare-empty' else check_card)
        else:
            check_pair(args.source, args.target)
            if bool(args.deleted) != bool(args.empty):
                raise ValueError('deletion acceptance requires both deleted and empty runs')
            if args.deleted:
                check_deletion(args.source, args.deleted, args.empty)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 offline SMS: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own offline SMS draft; network delivery unproved')


if __name__ == '__main__':
    main()
