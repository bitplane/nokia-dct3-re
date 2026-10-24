"""Own NSB-3 physical FDN On/cold-On/Off/cold-Off settings contract."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.sim_fdn_profile import extend
from tools.sim_fdn_protocol_check import check_protocol_phase
from tools.sim_security_trace_check import padded_pin
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME

PREFIX = ('Scroll Down',) * 3 + ('Left Softkey / Menu', 'Scroll Down',
    'Left Softkey / Menu', 'Scroll Down', 'Scroll Down', 'Scroll Down')


def expected_card(enabled=False):
    card = bytearray(extend(make_current_profile(True), True))
    card[3575] = 0 if enabled else 1
    return bytes(card)


def check_card(card, enabled=False):
    if card != expected_card(enabled):
        raise ValueError('complete FDN card differs beyond qualified validity state')


def check_trace(text, phase):
    if phase not in range(4) or 'nsb3_board_reset_request:' in text:
        raise ValueError('unknown FDN phase or observed reset')
    verifies = () if phase % 2 else ((0x20, 0, 2, 8, padded_pin('5678')),)
    check_pin(text, True, additional_verifies=verifies)
    keys = PREFIX if phase % 2 else PREFIX + ('Left Softkey / Menu',
        'Scroll Up' if phase == 0 else 'Scroll Down', 'Left Softkey / Menu') + tuple(
            'Keypad ' + digit for digit in '5678') + ('Left Softkey / Menu',)
    contacts = re.findall(r'nsb3_security_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if contacts != [(key, state) for key in keys for state in ('1', '0')]:
        raise ValueError('physical FDN settings contacts differ')
    values = re.findall(r'nsb3_sim_wire: direction=TX data=([0-9a-f]{2}) ', text)
    if not values:
        raise ValueError('missing SIM TX')
    check_protocol_phase(text, phase, bytes.fromhex(''.join(values)))


def check_fixture(source, run):
    before = (source / 'nvram/nsb3r4t/sim_card').read_bytes()
    if before != make_current_profile(True):
        raise ValueError('requires qualified original PIN-enabled laboratory source')
    metadata = json.loads((run / 'cold_source.json').read_text())
    if metadata.get('copied_guest_ram_or_save_state') is not False:
        raise ValueError('guest state must not be copied')
    for name in ('flash', 'eeprom', 'sim_card'):
        original = (source / 'nvram/nsb3r4t' / name).read_bytes()
        if metadata['storage_sha256'][name] != hashlib.sha256(original).hexdigest():
            raise ValueError('own source hash differs')
    card = expected_card()
    if (run / 'initial_sim_card.bin').read_bytes() != card:
        raise ValueError('external card extension differs')
    if metadata['external_card_fixture'] != {
            'kind': 'explicit FDN extension with invalid-ADN read access',
            'base_sha256': hashlib.sha256(before).hexdigest(),
            'input_sha256': hashlib.sha256(card).hexdigest(),
            'handset_storage_changed': False}:
        raise ValueError('explicit card fixture provenance differs')


def check_chain(runs):
    if len(runs) != 5:
        raise ValueError('requires base/On/cold-On/Off/cold-Off')
    check_fixture(runs[0], runs[1])
    for phase, run in enumerate(runs[1:]):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        if phase:
            check_link(runs[phase], run)
        check_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(), phase < 2)
        check_trace((run / 'error.log').read_text(), phase)
        frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
        frame_check(run / f'snap/nsb3_security_{9 if phase % 2 else 17:02d}.png',
                    'aed44705c9a6e9f8d18ba34092b4775f8ed74168b9c60f5ed4f1a9bff3ebbd72'
                    if phase < 2 else
                    '91dae780554d524047135c635fb02f28ca1dc26cd40acf834e2fcb8f678fe6eb')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-on-cold', 'prepare-off-cold', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires source and cold destination')
            prepare_reload(*args.runs, lambda card: check_card(card, args.operation == 'prepare-on-cold'))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 FDN: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own FDN settings/cold persistence; radio unqualified')


if __name__ == '__main__':
    main()
