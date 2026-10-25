"""Own NSB-1 clean-idle native save/load and physical Phone book replay."""
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
from tools.nsb1_diagnostic_display_check import card_check, contract, input_signature
from tools.nsb1_rom4_compare import INPUTS, check_card_run, check_cold_link, prepare_cold_run
from tools.nsb1_pin_roundtrip import check_correct
from tools.nse2_native_state_check import check_state

EEPROM = '4bd101b1c9089fed802aa5630135c4371f3bb070b136c7bfc15031917a598cad'
MENU = 'ed5b9d247f48525c5f67b4ca71914bff1802af857d212e4dd5cb0e7e8c9a8b33'


def check_storage(directory, *, eeprom_sha256=None):
    if eeprom_sha256 is None:
        eeprom_sha256=EEPROM
    storage = directory / 'nvram/nsb1r4t'
    files = {name: (storage / name).read_bytes() for name in ('flash', 'eeprom', 'sim_card')}
    contract(files['flash'])
    card_check(files['sim_card'])
    if len(files['eeprom']) != 8192 or hashlib.sha256(files['eeprom']).hexdigest() != eeprom_sha256:
        raise ValueError('complete physically selected own EEPROM differs')
    return files


def check_trace(text, before, after, reference, restored):
    if text.count('nsb1_state: phase=') != 2:
        raise ValueError('architecture observation count differs')
    result = check_state(text, before, after, reference, restored, prefix='nsb1',
                         checkpoint='40.000000000', menu_frame=MENU)
    if re.search(r'illegal instruction|unsupported|unhandled|fatal|LUA ERROR|Error in', text, re.I):
        raise ValueError('execution or observation failed')
    if 'nsb1_reset_reason:' in text:
        raise ValueError('baseband reset interrupted restoration')
    return result


def check(seed, run):
    files = check_storage(seed)
    check_storage(run)
    check_cold_link(run, seed)
    if input_signature(seed / 'cfg/nsb1r4t.cfg') != input_signature(run / 'cfg/nsb1r4t.cfg'):
        raise ValueError('external configuration changed')
    code = json.loads((run / 'exit_status.json').read_text())['returncode']
    text, card = check_card_run(run, code, 54)
    check_correct(text, card, run / 'snap')
    pixels = []
    for phase in ('reference', 'restored'):
        with Image.open(run / f'snap/nsb1_state_{phase}.png') as image:
            if image.size != (84, 48):
                raise ValueError('own LCD geometry differs')
            pixels.append(image.convert('RGB').tobytes())
    result = check_trace(text, files['sim_card'], card, *pixels)
    saved = run / 'sta/nsb1r4t/nsb1_native_ui.sta'
    if not saved.is_file() or not saved.stat().st_size:
        raise ValueError('nonempty saved artifact missing')
    result['save_sha256'] = hashlib.sha256(saved.read_bytes()).hexdigest()
    return result


def execute(binary, seed, run):
    check_storage(seed)
    prepare_cold_run(run, seed, card_check, INPUTS)
    script = Path(__file__).with_name('nsb1_native_state.lua').resolve()
    command = [str(binary), 'nsb1r4t', '-rompath', str(run / 'inputs'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-debug', '-debugger', 'none', '-log', '-verbose', '-seconds_to_run', '54',
               '-autoboot_delay', '0', '-autoboot_script', str(script)]
    (run / 'state_invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()), indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output,
                                stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


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
        parser.exit(1, f'NSB-1 native restoration FAIL: {error}\n')
    print(f"NSB-1 native restoration PASS: {result['io_replay_events']} repeated DSP I/O writes and physical Phone book; RF/speech unproved")


if __name__ == '__main__':
    main()
