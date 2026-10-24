"""Own NSK-1 Toolkit refusal diagnostic; EEPROM integrity remains unresolved."""
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
from tools.nsk1_native_state_check import check_seed, MENU
from tools.nsk1_diagnostic_display_check import (
    check_authentication, check_bank, check_link, contract, signature, IDLE,
)
from tools.nsk1_phonebook_roundtrip import copy_persistent, frame_check
from tools.nsk1_security_compare import check_phase, run_machine
from tools.make_sim_card_profile import make_current_profile
from tools.sim_toolkit_display_check import check_display

PROFILE = bytes.fromhex('03018000')
CHANGED_BANK = '1bcc40818ee276be75f40554240f5f2e5a94da8a2e85af0e9537ef9a1bc2ee0d'
NV_RECORDS = {0xa89fc: '0000072805500021', 0xa8a04: '0000072906580021',
              0xa8a0c: '0000072a06fd0021'}


def check_record_catalogue(image):
    for offset, encoded in NV_RECORDS.items():
        expected = bytes.fromhex(encoded)
        if image[offset:offset+len(expected)] != expected:
            raise ValueError('own NV record catalogue differs')


def prepare(seed, run, control=False):
    check_seed(seed)
    copy_persistent(run, seed)
    path = run / 'cfg/nsk1r4t.cfg'
    tree = ET.parse(path)
    inputs = tree.find('./system/input')
    if inputs.find("port[@tag=':SATCFG']") is not None:
        raise ValueError('pre-existing external-card override')
    ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG', mask='15',
                  defvalue='0', value='0' if control else '1')
    tree.write(path, encoding='utf-8', xml_declaration=True)


def check_configuration(seed, run, control=False):
    original = signature(seed / 'cfg/nsk1r4t.cfg')
    actual = signature(run / 'cfg/nsk1r4t.cfg')
    card = ('port', sorted(dict(tag=':SATCFG', type='CONFIG', mask='15',
                               defvalue='0', value='0' if control else '1').items()), '')
    ports = [row for row in actual if row[0] == 'port' and ('tag', ':SATCFG') in row[1]]
    if ports:
        if ports != [card]:
            raise ValueError('external card profile differs')
        actual.remove(card)
    elif not control:
        raise ValueError('external card profile missing')
    # MAME sorts ports and omits a zero/default override when saving CFG.
    if actual != original:
        raise ValueError('unrelated external configuration changed')


def check_trace(text, control=False):
    check_display(text, prefix='nsk1', terminal_profile=PROFILE, control=control,
                  capability_refusal=True, reset_markers=('nsk1_restart_request:',))
    before = text.split('nsk1_toolkit_physical: action=dismiss down=1', 1)[0]
    check_authentication(before, True)
    if (re.findall(r'nsk1_code_key: value=([0-9a-f]{2}) ', text) != ['01','02','03','04','05','19','19']
            or text.count('nsk1_code_key:') != 7):
        raise ValueError('post-refusal Menu decoding differs')
    native = re.findall(r'nsk1_toolkit_native: sample=(\d+) pc=([0-9a-f]{6}) stopped=([01]) idle=([01]) observer_writes=0', text)
    if ([(row[0], row[2]) for row in native] != [('1','0'),('2','0')]
            or text.count('nsk1_toolkit_native:') != 2):
        raise ValueError('native observations missing or stopped')
    if re.search(r'illegal instruction|unimplemented|unsupported|unhandled|fatal|LUA ERROR|Error in', text, re.I):
        raise ValueError('execution or observer failed')
    if 'nsk1_restart' in text or re.search(r'mad2_(?:clock|ledger):[^\n]*W off=01 data=05', text):
        raise ValueError('unexpected baseband restart')


def check_bank_transition(before, after, control=False):
    check_bank(before, True)
    if control:
        if after != before:
            raise ValueError('Phase-2 control changed EEPROM')
    elif len(after) != 2048 or hashlib.sha256(after).hexdigest() != CHANGED_BANK:
        raise ValueError('known unqualified EEPROM transition changed')
    return dict(eeprom_changed_bytes=sum(a != b for a,b in zip(before,after)),
                eeprom_integrity_validated=False)


def check(seed, run, control=False):
    files = check_seed(seed)
    check_link(run, seed)
    check_configuration(seed, run, control)
    if json.loads((run / 'exit_status.json').read_text()) != dict(returncode=0):
        raise ValueError('MAME did not complete')
    storage = run / 'nvram/nsk1r4t'
    image = (storage / 'flash').read_bytes()
    contract(image)
    check_record_catalogue(image)
    if (storage / 'sim_card').read_bytes() != make_current_profile(False):
        raise ValueError('complete original card changed')
    result = check_bank_transition(files['eeprom'], (storage / 'eeprom').read_bytes(), control)
    _, text = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
    check_trace(text, control)
    frame_check(run / 'snap/nsk1_toolkit_before.png', IDLE)
    frame_check(run / 'snap/nsk1_toolkit_after.png', MENU)
    return result


def execute(binary, seed, run, control=False):
    prepare(seed, run, control)
    script = Path(__file__).with_name('nsk1_toolkit_input.lua')
    command = run_machine(binary, run, script, 74, True)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=0))+'\n')
    (run / 'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()), indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--control', action='store_true')
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        seed, run = args.seed.resolve(), args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(), seed, run, args.control)
        result = check(seed, run, args.control)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 Toolkit diagnostic FAIL: {error}\n')
    print('NSK-1 Toolkit diagnostic reproduced: ' + ('Phase-2 control' if args.control else 'automatic result30 refusal'))
    print(json.dumps(result))
    print('NOT DISPLAY TEXT support or EEPROM-integrity acceptance')


if __name__ == '__main__':
    main()
