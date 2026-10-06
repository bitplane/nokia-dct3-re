"""Fresh isolated NPE-3 own-upload and research-HLE graphical acceptance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import shutil

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6210_upload_contract import assess
from tools.noki6210_staged_check import verify

SCENARIOS = {'stage': ('npe3stage', 'staged_observe', 12),
             'runtime': ('npe3hle', 'staged_observe', 18),
             'menu': ('npe3hle', 'menu_input', 25),
             'calculator': ('npe3hle', 'application_input', 38),
             'phonebook': ('npe3hle', 'phonebook_input', 33)}
MENU_SHA256 = '8c7650fdb0514ec34c85b89795e529de062e6f141268a507bafc7eb77370df65'
CALCULATOR_SHA256 = '2c5e99fd98ab56d41574c613021a7ed5270fe7d39e94ec57a1f52b9f732199fc'
CONTACT_SHA256 = '39ca7b13f4afdc8c6e3ca553d7fd0bafcdd7dd3de42c054edf0f445713dd09bc'


def events(path):
    with path.open(errors='replace') as stream:
        return ''.join(line for line in stream if any(token in line for token in
                      ('staged_dsp:', '6210_', 'dspif_transport:', 'sim_device:',
                       'SIM status', '[LUA ERROR]')))


def check_frame(frame, digest, description):
    if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
        raise ValueError(f'frame differs from reviewed {description}')


def check_calculator(text, frame):
    cursor = 0
    for action in ('application', 'input_1', 'input_12', 'operation_options',
                   'subtract', 'minus', 'input_3', 'options', 'result'):
        event = f'6210_application_physical: action={action}'
        cursor = text.find(event, cursor)
        if cursor < 0:
            raise ValueError(f'missing ordered Calculator input: {action}')
        cursor += len(event)
    check_frame(frame, CALCULATOR_SHA256, 'Calculator 12 - 3 = 9')


def check_phonebook(write_trace, read_trace, storage, frame):
    from tools.sim_phonebook_check import validate_phonebook_storage
    cursor = 0
    for event in ('6210_phonebook_physical: action=save',
                  'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
                  'body ins=dc length=32 selected=6f3a',
                  'SIM status ins=dc sw=9000'):
        cursor = write_trace.find(event, cursor)
        if cursor < 0:
            raise ValueError(f'missing ordered SIM save: {event}')
        cursor += len(event)
    if 'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in read_trace:
        raise ValueError('cold process did not read EF_ADN record 1')
    if '6210_phonebook_read_physical: action=contact' not in read_trace or 'ins=dc' in read_trace:
        raise ValueError('cold read must physically select contact without writing SIM')
    validate_phonebook_storage(storage, b'A')
    check_frame(frame, CONTACT_SHA256, 'cold A / 123 contact')


def check_menu(text, frame):
    import re
    from tools.radio_call_lifecycle_common import require_ordered
    records = [int(value, 16) for value in re.findall(
        r'sim_device: header cla=a0 ins=b2 p1=([0-9a-f]{2}) p2=04 p3=20 selected=6f3a', text)]
    if records != list(range(1, 51)):
        raise ValueError('expected complete own SIM initialization/50 ADN reads')
    require_ordered(text, (
        ('physical Menu', re.compile('6210_menu_physical: press=1')),
        ('own decoder', re.compile('6210_keypad_decoded: key=19')),
    ), '6210 physical input')
    if frame.size != (96, 60) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != MENU_SHA256:
        raise ValueError('Menu frame differs from reviewed Messages screen')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--scenario', choices=SCENARIOS, default='menu')
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        contract = assess((root / 'roms/noki6210/6210_556c.fls').read_bytes(),
                          (root / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
        run = args.run_directory.resolve()
        run.mkdir(parents=True, exist_ok=False)
        machine, script, seconds = SCENARIOS[args.scenario]
        command = [str((args.mame or root / 'mame/mame').resolve()), machine,
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-noreadconfig', '-debug', '-debugger', 'none',
                   '-autoboot_script', str(root / f'tools/noki6210_{script}.lua'),
                   '-autoboot_delay', '0', '-seconds_to_run', str(seconds),
                   '-video', 'none', '-sound', 'none', '-nothrottle', '-log', '-verbose']
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        text = events(run / 'error.log')
        runtime = args.scenario != 'stage'
        verify(text, runtime=runtime, selftest=runtime)
        if args.scenario == 'menu':
            from PIL import Image
            with Image.open(run / 'snap/6210_after_menu.png') as frame:
                check_menu(text, frame)
        elif args.scenario == 'calculator':
            from PIL import Image
            with Image.open(run / 'snap/6210_calculator_result.png') as frame:
                check_calculator(text, frame)
        elif args.scenario == 'phonebook':
            from PIL import Image
            cold = run / 'cold'
            cold.mkdir()
            shutil.copytree(run / 'nvram', cold / 'nvram')
            cold_command = command.copy()
            cold_command[cold_command.index('-autoboot_script') + 1] = str(root / 'tools/noki6210_phonebook_read.lua')
            cold_command[cold_command.index('-seconds_to_run') + 1] = '27'
            with (cold / 'console.log').open('w') as output:
                subprocess.run(cold_command, cwd=cold, stdout=output, stderr=subprocess.STDOUT,
                               check=True, timeout=180)
            read_trace = events(cold / 'error.log')
            verify(read_trace, runtime=True, selftest=True)
            with Image.open(cold / 'snap/6210_phonebook_read_contact.png') as frame:
                check_phonebook(text, read_trace,
                               (cold / 'nvram/npe3hle/sim_card').read_bytes(), frame)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': machine, 'scenario': args.scenario, 'passed': True,
            'provisioning': 'unchanged acquired product PMM', 'contract': contract,
            'native_dsp_complete': False, 'speech_tested': False, 'command': command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6210 acceptance FAIL: {error}\n')
    print(f'6210 {args.scenario} acceptance PASS: {run}')


if __name__ == '__main__':
    main()
