"""Own NSK-1 clean-idle native restoration and physical Phone book replay."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_diagnostic_display_check import (
    check_authentication, check_bank, check_link, contract, signature, IDLE,
)
from tools.make_sim_card_profile import make_current_profile
from tools.nsk1_phonebook_roundtrip import copy_persistent, frame_check
from tools.nsk1_security_compare import check_phase, run_machine
from tools.nse2_native_state_check import check_state

MENU = '99915d05551950033d0cfbfde4d35b743a5f75729eb3e9212d665df521fda193'


def check_storage(directory):
    storage = directory / 'nvram/nsk1r4t'
    files = {name: (storage / name).read_bytes() for name in ('flash','eeprom','sim_card')}
    contract(files['flash'])
    check_bank(files['eeprom'], True)
    if files['sim_card'] != make_current_profile(False):
        raise ValueError('complete original PIN-disabled laboratory card differs')
    return files


def check_trace(text, before, after, reference, restored):
    if text.count('nsk1_state: phase=') != 2:
        raise ValueError('native architecture observation count differs')
    result = check_state(text, before, after, reference, restored, prefix='nsk1',
                         checkpoint='40.000000000', menu_frame=MENU)
    check_authentication(text.split('nsk1_state: phase=saved', 1)[0], True)
    codes = re.findall(r'nsk1_code_key: value=([0-9a-f]{2}) ', text)
    if codes != ['01','02','03','04','05','19','19','19'] or text.count('nsk1_code_key:') != 8:
        raise ValueError('authentication and both physical Menu decodes differ')
    if re.search(r'illegal instruction|unimplemented|unsupported|unhandled|fatal|LUA ERROR|Error in', text, re.I):
        raise ValueError('execution or observation failed')
    if 'nsk1_restart' in text or re.search(r'mad2_(?:clock|ledger):[^\n]*W off=01 data=05', text):
        raise ValueError('unexpected baseband restart')
    return result


def check_seed(seed):
    files = check_storage(seed)
    _, text = check_phase(seed, (seed / 'initial_eeprom.bin').read_bytes(), through=40, minimum_records=1)
    check_authentication(text, True)
    frame_check(seed / 'snap/nsk1_idle_42.png', IDLE)
    signature(seed / 'cfg/nsk1r4t.cfg')
    return files


def check(seed, run):
    files = check_seed(seed)
    check_storage(run)
    check_link(run, seed)
    if signature(seed / 'cfg/nsk1r4t.cfg') != signature(run / 'cfg/nsk1r4t.cfg'):
        raise ValueError('external hardware inputs changed')
    if json.loads((run / 'exit_status.json').read_text()) != dict(returncode=0):
        raise ValueError('MAME did not complete')
    _, text = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=50, minimum_records=1)
    pixels = []
    for phase in ('reference','restored'):
        with Image.open(run / f'snap/nsk1_state_{phase}.png') as frame:
            if frame.size != (84,48):
                raise ValueError('own LCD geometry differs')
            pixels.append(frame.convert('RGB').tobytes())
    result = check_trace(text, files['sim_card'], (run / 'nvram/nsk1r4t/sim_card').read_bytes(), *pixels)
    saved = run / 'sta/nsk1r4t/nsk1_native_ui.sta'
    if not saved.is_file() or not saved.stat().st_size:
        raise ValueError('nonempty native save artifact missing')
    result['save_sha256'] = hashlib.sha256(saved.read_bytes()).hexdigest()
    return result


def execute(binary, seed, run):
    check_seed(seed)
    copy_persistent(run, seed)
    script = Path(__file__).with_name('nsk1_native_state.lua')
    command = run_machine(binary, run, script, 54, True)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=0))+'\n')
    (run / 'state_invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()), indent=2)+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        seed, run = args.seed.resolve(), args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(), seed, run)
        result = check(seed, run)
    except (OSError, ValueError, KeyError, ET.ParseError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSK-1 native restoration FAIL: {error}\n')
    print(f"NSK-1 native restoration PASS: {result['io_replay_events']} repeated DSP I/O writes and physical Phone book; RF/speech unproved")


if __name__ == '__main__':
    main()
