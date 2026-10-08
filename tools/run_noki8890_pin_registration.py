"""Isolated own-PMM 8890 physical PIN and configured GSM900 registration."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.sim_security_trace_check import validate
from tools.noki8890_staged_check import verify as verify_stage
from tools.noki8890_registration_check import verify as verify_registration


def check_pin_inputs(text):
    position = 0
    for key, code in (('Keypad 1', '01'), ('Keypad 2', '02'),
                      ('Keypad 3', '03'), ('Keypad 4', '04'), ('Menu', '19')):
        match = re.search(r'8890_pin_physical: key=' + re.escape(key) +
                          r'.*?8890_keypad_decoded: key=' + code + r'\b', text[position:], re.S)
        if not match:
            raise ValueError('missing physical PIN decode: ' + key)
        position += match.end()
    if not re.search(r'SIM status ins=20 sw=9000.*?'
                     r'LAPDm Location Updating Accept acknowledged nr=1', text[position:], re.S):
        raise ValueError('registration did not follow physical PIN acceptance')
    if not re.search(r'TX packet type=57 payload=4 .*data=01140000.*?'
                     r'RX enqueue type=8b payload=166 .*data=0010003c00c4.*?'
                     r'8890_band_rx: object=([0-9a-f]+).*?'
                     r'8890_band_parse: object=\1 arfcn=003c rssi=c4', text, re.S):
        raise ValueError('missing own late measurement response/consumer correlation')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--without-pin', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8890']
    try:
        roms = root / 'roms/noki8890'
        verify_inputs('8890', (roms / profile[1]).read_bytes(), (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        for directory in ('cfg', 'nvram/nsb6hle', 'snap'):
            (run / directory).mkdir(parents=True)
        shutil.copyfile(root / 'fixtures/noki8890_host/nsb6hle.cfg', run / 'cfg/nsb6hle.cfg')
        card = run / 'nvram/nsb6hle/sim_card'
        card.write_bytes(make_profile(pin_enabled=not args.without_pin))
        environment = os.environ.copy()
        environment.pop('NOKIA_DCT3_8890_PIN_ENTRY', None)
        if not args.without_pin:
            environment['NOKIA_DCT3_8890_PIN_ENTRY'] = '1'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki8890_security_input.lua'),
                   '-seconds_to_run', '46']
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=run, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
        text = (run / 'error.log').read_text(errors='replace')
        if not args.without_pin:
            validate(text, card.read_bytes(), 'verify', '1234')
            check_pin_inputs(text)
        verify_stage(text, runtime=True, selftest=True)
        verify_registration(text, configured_gsm900=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsb6hle', 'sim_pin_enabled': not args.without_pin,
            'provisioning': 'own acquired PMM unchanged', 'command': command,
            'native_dsp_complete': False, 'speech_tested': False, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 PIN registration FAIL: {error}\n')
    print(f'8890 PIN registration PASS: {run}')


if __name__ == '__main__':
    main()
