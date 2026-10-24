"""Qualify NSB-3 startup-text provisioning and physical Field test exit."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_menu_check import check_trace as check_security_menu

IDLE_FRAME = '192aec2de3cd98478979cc093ad09b2f7ad825bcfc29ac2162f64ba7f37ea462'
TEST_EDITOR_FRAME = 'a91940797ace29f2eab2c08c564a059ac958a3734ce6b11694da20c2fe34740d'
KEYS = ('Scroll Up', 'Left Softkey / Menu', 'Keypad 0', 'Keypad 0',
        'Left Softkey / Menu', 'End')


def contract(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires qualified own NSB-3 executable')
    for offset, encoded in (
        (0xd0340, '0000060404a10001'),
        (0xd0590, '0000074505ec001f'),
        (0xab0ac, '48371c0449372200'),
        (0xab0c2, '78202800d004'),
        (0x9d58a, '780b2b00d00e4fa5006e683c005b5ae3'),
        (0xe024e, '017e'),
    ):
        expected = bytes.fromhex(encoded)
        if image[offset:offset + len(expected)] != expected:
            raise ValueError(f'own startup-text/diagnostic contract differs at {offset:#x}')


def empty_startup_text(eeprom):
    if (len(eeprom) != 8192 or eeprom[0x5ec:0x60b] != b'\xff' * 31
            or eeprom[0x4a1] != 0xff):
        raise ValueError('requires erased startup text and original diagnostic selector')
    result = bytearray(eeprom)
    result[0x5ec] = 0
    return bytes(result)


def check_card(card):
    if card != make_current_profile(True):
        raise ValueError('requires unchanged original PIN-enabled laboratory card')


def prepare(source, target):
    storage = source / 'nvram/nsb3r4t'
    contract((storage / 'flash').read_bytes())
    eeprom = empty_startup_text((storage / 'eeprom').read_bytes())
    prepare_reload(source, target, check_card)
    (target / 'nvram/nsb3r4t/eeprom').write_bytes(eeprom)
    (target / 'initial_eeprom.bin').write_bytes(eeprom)
    metadata = {'record': '0x0745', 'changed_offsets': ['0x5ec'],
                'empty_first_byte': True, 'factory_provisioning_claim': False,
                'diagnostic_selector_changed_by_fixture': False}
    (target / 'startup_text_fixture.json').write_text(json.dumps(metadata, indent=2) + '\n')


def frame(path):
    from PIL import Image
    with Image.open(path) as image:
        if image.size != (84, 48):
            raise ValueError('idle frame extent differs')
        return hashlib.sha256(image.convert('RGB').tobytes()).hexdigest()


def inputs(path):
    system = ET.parse(path).getroot().find('system')
    if system is None or system.get('name') != 'nsb3r4t' or system.find('input') is None:
        raise ValueError('own machine input configuration missing')
    return [(node.tag, sorted(node.attrib.items()), (node.text or '').strip())
            for node in system.find('input').iter()]


def check(source, baseline, run, cold):
    source_storage = source / 'nvram/nsb3r4t'
    original_inputs = inputs(source / 'cfg/nsb3r4t.cfg')
    check_card((source_storage / 'sim_card').read_bytes())
    original = (source_storage / 'eeprom').read_bytes()
    expected = bytearray(empty_startup_text(original))
    if (baseline / 'initial_eeprom.bin').read_bytes() != expected:
        raise ValueError('startup-text fixture differs from one-byte external input')
    metadata = json.loads((baseline / 'startup_text_fixture.json').read_text())
    if metadata != {'record': '0x0745', 'changed_offsets': ['0x5ec'],
                    'empty_first_byte': True, 'factory_provisioning_claim': False,
                    'diagnostic_selector_changed_by_fixture': False}:
        raise ValueError('startup-text provenance differs')
    # Prime ordinary firmware-owned diagnostic records before isolating the
    # physical selector write. Do not whitelist those record ranges as noise.
    primed = (baseline / 'nvram/nsb3r4t/eeprom').read_bytes()
    if len(primed) != 8192 or primed[0x5ec] != 0 or primed[0x4a1] != 0xff:
        raise ValueError('baseline startup text or diagnostic selector differs')
    expected = bytearray(primed)
    expected[0x4a1] = 0x41
    for directory, predecessor in ((baseline, source), (run, baseline), (cold, run)):
        if inputs(directory / 'cfg/nsb3r4t.cfg') != original_inputs:
            raise ValueError('external input configuration changed')
        link = json.loads((directory / 'cold_source.json').read_text())
        if (Path(link['source']).resolve() != predecessor.resolve()
                or link['copied_guest_ram_or_save_state'] is not False):
            raise ValueError('independent cold-storage link differs')
        for name in ('flash', 'eeprom', 'sim_card'):
            digest = hashlib.sha256((predecessor / 'nvram/nsb3r4t' / name).read_bytes()).hexdigest()
            if link['storage_sha256'][name] != digest:
                raise ValueError('cold predecessor storage hash differs')
        storage = directory / 'nvram/nsb3r4t'
        image = (storage / 'flash').read_bytes()
        contract(image)
        if image != (source_storage / 'flash').read_bytes():
            raise ValueError('own flash changed')
        card = (storage / 'sim_card').read_bytes()
        check_card(card)
        if (directory / 'initial_sim_card.bin').read_bytes() != card:
            raise ValueError('card changed during run')
        if directory != baseline and (storage / 'eeprom').read_bytes() != expected:
            raise ValueError('EEPROM differs from startup-text/physical diagnostic changes')
        text = (directory / 'error.log').read_text()
        check_security_menu(text)
        for digit in '1234':
            if f'nsb3_pin_key: correct=1 key=Keypad {digit} down=1' not in text:
                raise ValueError('physical SIM authentication missing')
        if 'nsb3_board_reset_request:' in text:
            raise ValueError('unexpected native reset')
    text = (run / 'error.log').read_text()
    rows = re.findall(r'nsb3_fieldtest_key: key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
    if rows != [(key, state) for key in KEYS for state in ('1', '0')]:
        raise ValueError('physical Field test selection differs')
    if frame(run / 'snap/nsb3_fieldtest_02.png') != TEST_EDITOR_FRAME:
        raise ValueError('reviewed Test editor pixels differ')
    for path in (run / 'snap/nsb3_fieldtest_05.png', run / 'snap/nsb3_fieldtest_06.png',
                 cold / 'snap/nsb3_phonecode_later_8.png'):
        if frame(path) != IDLE_FRAME:
            raise ValueError('reviewed idle-layout pixels differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('source', type=Path)
    prep.add_argument('target', type=Path)
    copy = sub.add_parser('cold')
    copy.add_argument('source', type=Path)
    copy.add_argument('target', type=Path)
    verify = sub.add_parser('check')
    for name in ('source', 'baseline', 'run', 'cold'):
        verify.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'prepare':
            prepare(args.source, args.target)
        elif args.command == 'cold':
            contract((args.source / 'nvram/nsb3r4t/flash').read_bytes())
            prepare_reload(args.source, args.target, check_card)
        else:
            check(args.source, args.baseline, args.run, args.cold)
    except (OSError, ValueError, KeyError, ET.ParseError) as exc:
        parser.exit(1, f'NSB-3 idle profile: FAIL: {exc}\n')
    print('PASS: explicit empty startup text; physical diagnostic exit and cold idle pixels' if args.command == 'check'
          else 'Copied cold own storage without guest RAM/state' if args.command == 'cold'
          else 'Prepared one-byte external startup-text fixture; diagnostic state left firmware-owned')


if __name__ == '__main__':
    main()
