"""Qualify physical NSB-1 diagnostic-display exit and cold idle pixels."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_current_profile
from tools.nsb1_pin_roundtrip import check_correct
from tools.nsb1_rom4_compare import INPUTS, check_card_run, check_cold_link, check_frame, prepare_cold_run, run_mame

IDLE = '934b3dcd1024275cbee8dc1e87ec35124fb617a917a0684a1e7bc1463ee59068'
EARLY_IDLE = 'ef4e95ef2012184a6461445e0a5ebefac92cf0887f6d7409643a36947b11f21e'
FIELD_TEST = '15970bd84bf9c79a6628ee62eafb7fc92baeb0a28b05857bfbb44322dab695d7'
TEST_EDITOR = 'ae4639254eca78648d99735ad9a16bd5b2e7b6510031ea1681365050bf2e5170'
KEYS = ('menu', 'up', 'menu', 'zero', 'zero', 'menu', 'back', 'back')


def contract(image):
    if hashlib.sha1(image).hexdigest() != INPUTS[0][1]:
        raise ValueError('requires own NSB-1 v6.71 PPM A')
    for address, encoded in ((0x2cc088, '0000060404a10001'),
                             (0x2cc2d8, '0000074505ec001f')):
        expected = bytes.fromhex(encoded)
        offset = address-0x200000
        if image[offset:offset+len(expected)] != expected:
            raise ValueError(f'own EEPROM descriptor differs at {address:#x}')


def card_check(card):
    if card != make_current_profile(True):
        raise ValueError('complete original PIN-enabled laboratory card differs')


def selected_eeprom(original):
    if len(original) != 8192 or original[0x4a1] != 0xff or original[0x5ec:0x60b] != b'\xff'*31:
        raise ValueError('requires primed own storage with erased text and original diagnostic selector')
    expected = bytearray(original)
    expected[0x4a1] = 0x41
    return bytes(expected)


def input_signature(path):
    system = ET.parse(path).find('system')
    if system is None or system.get('name') != 'nsb1r4t' or system.find('input') is None:
        raise ValueError('own input configuration missing')
    return [(node.tag, sorted(node.attrib.items()), (node.text or '').strip())
            for node in system.find('input').iter()]


def check_actions(text):
    keys = re.findall(r'nsb1_diagnostic_key: key=(\w+) down=([01]) t=[0-9.]+ physical_only=1', text)
    if keys != [(key, state) for key in KEYS for state in ('1', '0')]:
        raise ValueError('physical diagnostic-display sequence differs')


def check(source, run, cold):
    storage = source / 'nvram/nsb1r4t'
    contract((storage / 'flash').read_bytes())
    card_check((storage / 'sim_card').read_bytes())
    expected = selected_eeprom((storage / 'eeprom').read_bytes())
    config = input_signature(source / 'cfg/nsb1r4t.cfg')
    for directory, predecessor, seconds in ((run, source, 54), (cold, run, 48)):
        check_cold_link(directory, predecessor)
        if input_signature(directory / 'cfg/nsb1r4t.cfg') != config:
            raise ValueError('external hardware inputs changed')
        code = json.loads((directory / 'exit_status.json').read_text())['returncode']
        text, card = check_card_run(directory, code, seconds)
        card_check(card)
        check_correct(text, card, directory / 'snap')
        if (directory / 'nvram/nsb1r4t/eeprom').read_bytes() != expected:
            raise ValueError('EEPROM differs from the single physical diagnostic selection')
        if any(marker in text for marker in ('nsb1_reset_reason:', 'LUA ERROR', 'Error in')):
            raise ValueError('reset or failed fixture')
        if directory == run:
            check_actions(text)
        elif 'nsb1_diagnostic_key:' in text:
            raise ValueError('cold idle repeated the diagnostic selection')
    check_frame(run / 'snap/nsb1_diagnostic_02.png', FIELD_TEST)
    check_frame(run / 'snap/nsb1_diagnostic_03.png', TEST_EDITOR)
    for index in (6, 7, 8):
        check_frame(run / f'snap/nsb1_diagnostic_{index:02}.png', IDLE)
    # The accessory title has not yet appeared at25s; retain that phase too.
    check_frame(cold / 'snap/nsb1_idle_25.png', EARLY_IDLE)
    for time in (36, 42):
        check_frame(cold / f'snap/nsb1_idle_{time}.png', IDLE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'run', 'cold'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--mame', type=Path, help='execute two fresh independent cold processes')
    args = parser.parse_args()
    source, run, cold = (getattr(args, name).resolve() for name in ('source', 'run', 'cold'))
    try:
        if args.mame:
            if run.exists() or cold.exists():
                raise ValueError('both output directories must be fresh')
            selected_eeprom((source / 'nvram/nsb1r4t/eeprom').read_bytes())
            for target, predecessor, script, seconds in (
                    (run, source, 'nsb1_diagnostic_display_input.lua', 54),
                    (cold, run, 'nsb1_idle_input.lua', 48)):
                prepare_cold_run(target, predecessor, card_check, INPUTS)
                script_path = Path(__file__).with_name(script).resolve()
                binary = args.mame.resolve()
                (target / 'idle_invocation.json').write_text(json.dumps({
                    'mame': str(binary), 'mame_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                    'script_sha256': hashlib.sha256(script_path.read_bytes()).hexdigest(),
                    'seconds': seconds, 'guest_state_copied': False,
                }, indent=2)+'\n')
                run_mame(binary, target / 'inputs', target, script_path, seconds, True)
        check(source, run, cold)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSB-1 diagnostic display FAIL: {error}\n')
    print('NSB-1 physical diagnostic exit/cold idle PASS; accessory, registration and native speech unproved')


if __name__ == '__main__':
    main()
