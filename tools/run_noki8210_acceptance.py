"""Isolated NSM-3 research-HLE acceptance with acquired base-record fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8210_pmm_check import base_record_fixture

SCENARIOS = {
    'registration': ('registration_input', 46, 'registration_check'),
    'outgoing-call': ('outgoing_call_input', 49, 'outgoing_call_check'),
    'incoming-call': ('incoming_call_input', 44, 'incoming_call_check'),
    'incoming-sms': ('incoming_sms_input', 32, 'incoming_sms_check'),
    'outgoing-sms': ('outgoing_sms_input', 46, 'outgoing_sms_check'),
    'calculator': ('calculator_input', 41, None),
    'phonebook': ('phonebook_input', 34, 'phonebook_check'),
}


MCU_SHA1 = 'c1a0fe95cedb89a92b19654208cc4855e1a4988e'
PMM_SHA256 = '31f51bcd69e183f23c39136574bd6a44864eb2ba1939417b9d848a3e0639ec59'


def prepare_run(run, mcu, pmm):
    """Pin provenance before creating storage; never reuse a previous run."""
    if hashlib.sha1(mcu).hexdigest() != MCU_SHA1:
        raise ValueError('unexpected acquired NSM-3 MCU/PPM image')
    if hashlib.sha256(pmm).hexdigest() != PMM_SHA256:
        raise ValueError('unexpected acquired NSM-3 PMM image')
    fixture = base_record_fixture(pmm)
    run.mkdir(parents=True, exist_ok=False)
    (run / 'nvram/nsm3hle').mkdir(parents=True)
    (run / 'cfg').mkdir()
    (run / 'nvram/nsm3hle/flash').write_bytes(mcu + fixture)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--scenario', choices=SCENARIOS, default='registration')
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        mcu = (root / 'roms/noki8210/8210_5.31ppm_c.fls').read_bytes()
        pmm = (root / 'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes()
        prepare_run(run, mcu, pmm)
        config = {'incoming-call': 'radio_incoming_call_answered',
                  'incoming-sms': 'radio_incoming_sms'}.get(args.scenario)
        if config:
            shutil.copyfile(root / f'fixtures/{config}/nsm3hle.cfg', run / 'cfg/nsm3hle.cfg')
        script, seconds, checker = SCENARIOS[args.scenario]
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsm3hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-noreadconfig', '-debug', '-debugger', 'none',
                   '-autoboot_script', str(root / f'tools/noki8210_{script}.lua'),
                   '-autoboot_delay', '0', '-seconds_to_run', str(seconds),
                   '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-verbose']
        def execute(cmd, output):
            with (run / output).open('w') as console:
                subprocess.run(cmd, cwd=run, stdout=console, stderr=subprocess.STDOUT, check=True)
        execute(command, 'console.log')
        check = [sys.executable, str(root / f'tools/noki8210_{checker}.py'), str(run / 'error.log')]
        storage = str(run / 'nvram/nsm3hle/sim_card')
        if args.scenario == 'phonebook':
            shutil.copyfile(run / 'error.log', run / 'write.log')
            cold = command.copy()
            cold[cold.index('-autoboot_script') + 1] = str(root / 'tools/noki8210_phonebook_read.lua')
            cold[cold.index('-seconds_to_run') + 1] = '28'
            execute(cold, 'cold_console.log')
            check = [sys.executable, str(root / 'tools/noki8210_phonebook_check.py'),
                     str(run / 'write.log'), str(run / 'error.log'), storage]
        elif args.scenario in ('registration', 'incoming-sms'):
            check.append(storage)
        if checker:
            subprocess.run(check, cwd=root, check=True)
        else:
            from PIL import Image
            frame = Image.open(run / 'snap/8210_calculator_result.png').convert('L')
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != CALCULATOR_SHA256:
                raise ValueError('calculator result differs from reviewed 15 frame')
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm3hle', 'scenario': args.scenario, 'passed': True,
            'provisioning': 'unchanged acquired base record; later low journal omitted',
            'native_dsp_complete': False, 'speech_tested': False,
            'command': command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'8210 acceptance FAIL: {error}\n')
    print(f'8210 {args.scenario} acceptance PASS: {run}')


CALCULATOR_SHA256 = '04ff410190e8c72f1fa6dbbb00c965051c520fee4b7ca6fe9c7d395ccdfc23e5'

if __name__ == '__main__':
    main()
