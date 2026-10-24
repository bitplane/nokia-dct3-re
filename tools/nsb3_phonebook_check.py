"""Qualify NSB-3 own-storage cold SIM contact retrieval, not radio service."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_menu_check import check_trace as check_security_menu
from tools.nsk3_phonebook_check import check_card, expected_record
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_fdn_protocol_check import apdus

SAVE_KEYS = ('Right Softkey / C', 'Right Softkey / C', 'Scroll Down',
             'Left Softkey / Menu', 'Keypad 2', 'Left Softkey / Menu',
             'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu')
READ_KEYS = ('Right Softkey / C', 'Right Softkey / C',
             'Left Softkey / Menu', 'Left Softkey / Menu', 'Left Softkey / Menu')


def check_contacts(text, readback=False):
    check_security_menu(text)
    rows = re.findall(r'nsb3_phonebook_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    expected = [(key, state) for key in (READ_KEYS if readback else SAVE_KEYS) for state in ('1', '0')]
    if rows != expected:
        raise ValueError('physical phonebook contact order differs')


def wire(text):
    values = re.findall(r'nsb3_sim_wire: direction=TX data=([0-9a-f]{2}) ', text)
    if not values or len(values) >= 8192:
        raise ValueError('missing or capped SIM TX stream')
    return list(apdus(bytes.fromhex(''.join(values))))


def fixture_origin(source, seen=None):
    source = source.resolve()
    seen = set() if seen is None else seen
    if source in seen:
        raise ValueError('cyclic own-storage provenance')
    seen.add(source)
    if (source / 'input.json').is_file():
        metadata = json.loads((source / 'input.json').read_text())
        if (not metadata.get('insert_sim') or not metadata.get('fresh_phone_retry_initialized')
                or metadata.get('image_sha256') != IMAGE_SHA256):
            raise ValueError('requires explicit own inserted-card retry fixture')
        return source
    provenance = json.loads((source / 'cold_source.json').read_text())
    if provenance.get('copied_guest_ram_or_save_state') is not False:
        raise ValueError('source cold-run provenance missing')
    return fixture_origin(Path(provenance['source']), seen)


def prepare_reload(source, target, validate_card=check_card):
    storage = source / 'nvram/nsb3r4t'
    flash = (storage / 'flash').read_bytes()
    if hashlib.sha256(flash).hexdigest() != IMAGE_SHA256:
        raise ValueError('source executable is not own qualified NSB-3 input')
    validate_card((storage / 'sim_card').read_bytes())
    origin = fixture_origin(source)
    eeprom = (storage / 'eeprom').read_bytes()
    if len(eeprom) != 8192:
        raise ValueError('source EEPROM extent differs')
    target.mkdir(parents=True, exist_ok=False)
    (target / 'nvram/nsb3r4t').mkdir(parents=True)
    (target / 'cfg').mkdir()
    hashes = {}
    for name in ('flash', 'eeprom', 'sim_card'):
        data = (storage / name).read_bytes()
        (target / 'nvram/nsb3r4t' / name).write_bytes(data)
        if name == 'sim_card':
            (target / 'initial_sim_card.bin').write_bytes(data)
        hashes[name] = hashlib.sha256(data).hexdigest()
    shutil.copyfile(source / 'cfg/nsb3r4t.cfg', target / 'cfg/nsb3r4t.cfg')
    (target / 'cold_source.json').write_text(json.dumps({
        'source': str(source.resolve()), 'storage_sha256': hashes,
        'fixture_origin': str(origin),
        'copied_guest_ram_or_save_state': False}, indent=2) + '\n')


def check_pair(save, cold):
    cards = []
    for run, readback in ((save, False), (cold, True)):
        text = (run / 'error.log').read_text()
        check_contacts(text, readback)
        commands = wire(text)
        updates = [row for row in commands if row[0] == 0xdc]
        if readback:
            if updates or not any(row[:4] == (0xb2, 1, 4, 32) and row[5] == 0x6f3a for row in commands):
                raise ValueError('cold run must read ADN record one without UPDATE RECORD')
        elif updates != [(0xdc, 1, 4, 32, expected_record(), 0x6f3a)]:
            raise ValueError('save must issue exactly the A/12345 ADN update')
        data = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        check_card(data)
        cards.append(data)
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
    if cards[0] != cards[1]:
        raise ValueError('cold read changed persisted card bytes')
    provenance = json.loads((cold / 'cold_source.json').read_text())
    if provenance.get('copied_guest_ram_or_save_state') is not False:
        raise ValueError('cold provenance missing')
    if provenance['storage_sha256']['sim_card'] != hashlib.sha256(cards[0]).hexdigest():
        raise ValueError('cold source card differs')
    for name in ('flash', 'eeprom'):
        if provenance['storage_sha256'][name] != hashlib.sha256((save / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest():
            raise ValueError('cold source handset storage differs')
    frame_check(save / 'snap/nsb3_phonebook_12.png', '8c01f87b60e87cb07ab22ffe2418b7a4510554681ede41c423e85abecaca5231')
    frame_check(cold / 'snap/nsb3_phonebook_04.png', 'aa51bf7de3f8a3034f5f001113e5ff51a8f17d4c62a75ee0778f34140d023ef1')
    frame_check(cold / 'snap/nsb3_phonebook_05.png', 'c107145ed4247dc6f298f7811d4c0400481827093abccb703d7f559825910813')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-reload', 'check'))
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'prepare-reload':
            prepare_reload(args.source, args.target)
        else:
            check_pair(args.source, args.target)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 phonebook: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own-storage SIM contact contract')


if __name__ == '__main__':
    main()
