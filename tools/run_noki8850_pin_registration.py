"""Isolated own-PMM 8850 physical SIM PIN and laboratory registration."""
import argparse
import json
import os
import re
from pathlib import Path
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.make_sim_card_profile import make_profile
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.sim_security_trace_check import validate
from tools.noki8850_staged_trace_check import (
    check_trace, check_physical_inputs, check_navigation_frames,
    check_correlated_inputs,
)


def check_pin_inputs(text):
    errors = check_correlated_inputs(text, tuple(
        ('pin_physical: key=' + key, decoded) for key, decoded in (
            ('Keypad 1', '01'), ('Keypad 2', '02'), ('Keypad 3', '03'),
            ('Keypad 4', '04'), ('Menu', '19'))))
    if not re.search(r'8850_pin_physical: key=Menu.*?SIM status ins=20 sw=9000.*?'
                     r'LAPDm Location Updating Accept acknowledged nr=1', text, re.S):
        errors.append('registration did not follow physical PIN acceptance')
    if errors:
        raise ValueError('; '.join(errors))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--without-pin', action='store_true',
                        help='run the original phone-code-only baseline')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8850']
    try:
        roms = root / 'roms/noki8850'
        verify_inputs('8850', (roms / profile[1]).read_bytes(),
                      (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        for directory in ('cfg', 'nvram/nsm2hle', 'snap'):
            (run / directory).mkdir(parents=True)
        card = run / 'nvram/nsm2hle/sim_card'
        card.write_bytes(make_profile(pin_enabled=not args.without_pin))
        environment = os.environ.copy()
        environment.pop('NOKIA_DCT3_8850_PIN_ENTRY', None)
        if not args.without_pin:
            environment['NOKIA_DCT3_8850_PIN_ENTRY'] = '1'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsm2hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki8850_security_input.lua'),
                   '-seconds_to_run', '46']
        with (run / 'console.log').open('w') as console:
            subprocess.run(command, cwd=run, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
        text = (run / 'error.log').read_text(errors='replace')
        if not args.without_pin:
            validate(text, card.read_bytes(), 'verify', '1234')
            check_pin_inputs(text)
        errors = (check_trace(text, runtime_hle=True, sim_reads=True) +
                  check_physical_inputs(text) + check_navigation_frames(run / 'snap'))
        if errors:
            raise ValueError('; '.join(errors))
        subprocess.run([sys.executable, str(root / 'tools/radio_registration_trace_check.py'),
                        str(run / 'error.log'), '--profile', 'nsm2'], check=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm2hle',
            'sim_profile': 'default laboratory card' if args.without_pin else 'PIN-enabled laboratory card',
            'provisioning': 'own acquired PMM unchanged',
            'native_dsp_complete': False, 'speech_tested': False,
            'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8850 PIN registration FAIL: {error}\n')
    print(f'8850 PIN registration PASS: {run}')


if __name__ == '__main__':
    main()
