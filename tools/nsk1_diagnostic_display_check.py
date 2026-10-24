"""Own NSK-1 physical Field test00 selection and independent cold idle."""
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
from tools.nsk1_phonebook_roundtrip import copy_persistent, frame_check
from tools.nsk1_security_compare import check as check_security, check_phase, run_machine
from tools.nsk1_rom4_compare import INPUTS

SOURCE = '5a35e87baa4727fcd0c99770aa54d0c6fc50ebebce806de81a28175f20bd2efb'
SELECTED = '6d31cb5b3d495145f6d0903147aeebf98571cd59b5adab538e03bc866693206e'
IDLE = '68fbc5929acbebe923d4df2456ca487e044cdde0e122e05d4eebbf5c9ce02fc2'
EARLY = '4cccad65b6731317fb75b609452abdbe24b04982265b04cfd916c48b76b12b45'
FRAMES = {
    2: '3f97af4479f62c3d521f67a21e884851af35ff1bbd8c4f8eadebf92269ed194d',
    3: 'c77b7975647cf241b1937af459858cb7d2cac169080f9172e2dca24925148515',
    4: 'ae4639254eca78648d99735ad9a16bd5b2e7b6510031ea1681365050bf2e5170',
    6: 'ed967b62b8cb97fe470d3c45e69991a2d435f187fb9526ddfe3ed7b813e9b9f7',
    7: IDLE, 8: IDLE, 9: IDLE,
}
ACTIONS = ('menu', 'up', 'menu', 'menu', 'zero', 'zero', 'menu', 'back', 'back')
TIMES = (28, 30, 32, 34, 37, 39, 41, 44, 46)


def contract(image):
    if hashlib.sha1(image).hexdigest() != INPUTS[0][1]:
        raise ValueError('own standard NSK-1 executable differs')
    if image[0xa889c:0xa88a4] != bytes.fromhex('0000060402990001'):
        raise ValueError('own0604 descriptor differs')


def check_bank(bank, selected=False):
    if (len(bank) != 2048 or bank[0x299] != (0x3d if selected else 0xff)
            or hashlib.sha256(bank).hexdigest() != (SELECTED if selected else SOURCE)):
        raise ValueError('complete own diagnostic-display bank differs')


def signature(path):
    system = ET.parse(path).find('system')
    if system is None or system.get('name') != 'nsk1r4t' or system.find('input') is None:
        raise ValueError('own hardware input configuration missing')
    return [(node.tag, sorted(node.attrib.items()), (node.text or '').strip())
            for node in system.find('input').iter()]


def check_authentication(text, cold=False):
    contacts = re.findall(r'nsk1_code_contact: index=(\d+) column=(\d) mask=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1', text)
    expected = [(str(i), str(column), f'{mask:02x}', str(down), 11+i+delay)
                for i, (column, mask) in enumerate(((2,2),(3,2),(4,2),(2,4),(3,4),(1,4)), 1)
                for down, delay in ((1,0),(0,.2))]
    if len(contacts) != 12 or text.count('nsk1_code_contact:') != 12:
        raise ValueError('physical authentication contact count differs')
    for row, wanted in zip(contacts, expected):
        if row[:4] != wanted[:4] or not wanted[4] <= float(row[4]) < wanted[4]+.1:
            raise ValueError('physical authentication contact differs')
    gates = re.findall(r'nsk1_code_gate: match=([0-9a-f]{8}) retry=([0-9a-f]{2}) ', text)
    if gates != [('00000001', '00')] or text.count('nsk1_code_gate:') != 1:
        raise ValueError('own phone authentication differs')
    actions = re.findall(r'nsk1_diagnostic_key: key=(\w+) down=([01]) t=([0-9.]+) physical_only=1', text)
    expected_actions = [] if cold else [(key, str(down), time+delay)
        for key, time in zip(ACTIONS, TIMES) for down, delay in ((1,0),(0,.2))]
    if len(actions) != len(expected_actions) or text.count('nsk1_diagnostic_key:') != len(actions):
        raise ValueError('physical diagnostic selection differs')
    for row, wanted in zip(actions, expected_actions):
        if row[:2] != wanted[:2] or abs(float(row[2])-wanted[2]) > .001:
            raise ValueError('physical selection order/time differs')
    codes = re.findall(r'nsk1_code_key: value=([0-9a-f]{2}) ', text)
    expected_codes = [1,2,3,4,5,0x19] + ([] if cold else [0x19,0x17,0x19,0x19,0xa,0xa,0x19,0x1a,0x1a])
    if codes != [f'{code:02x}' for code in expected_codes] or text.count('nsk1_code_key:') != len(codes):
        raise ValueError('own physical key decoding differs')
    if re.search(r'illegal instruction|unsupported|unhandled|fatal|LUA ERROR|Error in', text, re.I):
        raise ValueError('execution or observer failure')
    if 'nsk1_restart' in text or re.search(r'mad2_(?:clock|ledger):[^\n]*W off=01 data=05', text):
        raise ValueError('unexpected baseband restart')


def check_link(run, source):
    expected = dict(classification='cold continuation of qualified own persistent-input workflow',
        source_manifest_sha256=hashlib.sha256((source / 'input_manifest.json').read_bytes()).hexdigest(),
        source_card_sha256=hashlib.sha256((source / 'nvram/nsk1r4t/sim_card').read_bytes()).hexdigest(),
        copied_directories=['inputs','nvram','cfg'], ram_or_save_state_copied=False)
    if json.loads((run / 'input_manifest.json').read_text()) != expected:
        raise ValueError('cold provenance differs')
    if (run / 'initial_eeprom.bin').read_bytes() != (source / 'nvram/nsk1r4t/eeprom').read_bytes():
        raise ValueError('complete initial EEPROM differs')


def check_source(source):
    storage = source / 'nvram/nsk1r4t'
    check_bank((storage / 'eeprom').read_bytes())
    contract((storage / 'flash').read_bytes())
    _, text = check_phase(source, (storage / 'eeprom').read_bytes(), through=30, minimum_records=1)
    check_security(text, source / 'snap', True)
    card = make_current_profile(False)
    if (storage / 'sim_card').read_bytes() != card:
        raise ValueError('complete original PIN-disabled card differs')
    config = signature(source / 'cfg/nsk1r4t.cfg')
    return card, config


def check(source, run, cold):
    card, config = check_source(source)
    for directory, predecessor, through in ((run,source,50), (cold,run,40)):
        check_link(directory, predecessor)
        if signature(directory / 'cfg/nsk1r4t.cfg') != config:
            raise ValueError('external inputs changed')
        if json.loads((directory / 'exit_status.json').read_text()) != dict(returncode=0):
            raise ValueError('MAME did not complete')
        _, text = check_phase(directory, (directory / 'initial_eeprom.bin').read_bytes(),
                              through=through, minimum_records=1)
        check_authentication(text, directory == cold)
        if (directory / 'nvram/nsk1r4t/sim_card').read_bytes() != card:
            raise ValueError('complete SIM changed')
        check_bank((directory / 'nvram/nsk1r4t/eeprom').read_bytes(), True)
    for index, digest in FRAMES.items():
        frame_check(run / f'snap/nsk1_diagnostic_{index:02}.png', digest)
    for time, digest in ((25,EARLY),(36,IDLE),(42,IDLE)):
        frame_check(cold / f'snap/nsk1_idle_{time}.png', digest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source','run','cold'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        source, run, cold = (getattr(args, name).resolve() for name in ('source','run','cold'))
        if args.mame:
            check_source(source)
            for target, predecessor, script, seconds in (
                    (run,source,'nsk1_diagnostic_display_input.lua',54),
                    (cold,run,'nsk1_idle_input.lua',48)):
                copy_persistent(target, predecessor)
                command = run_machine(args.mame.resolve(), target, Path(__file__).with_name(script), seconds, True)
                (target / 'invocation.json').write_text(json.dumps(command, indent=2)+'\n')
                (target / 'exit_status.json').write_text(json.dumps(dict(returncode=0))+'\n')
        check(source, run, cold)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 diagnostic display FAIL: {error}\n')
    print('NSK-1 physical Field test00 and cold idle PASS; accessory, RF, registration and speech unproved')


if __name__ == '__main__':
    main()
