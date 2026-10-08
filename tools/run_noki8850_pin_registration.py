"""Isolated own-PMM 8850 physical SIM PIN and laboratory registration."""
import argparse
import json
import os
import re
import shutil
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
    parser.add_argument('--scenario', choices=('registration', 'host-incoming-call', 'host-incoming-sms',
                                              'host-outgoing-call', 'host-outgoing-sms', 'phonebook',
                                              'idle-state', 'call-state', 'sms-state'),
                        default='registration')
    parser.add_argument('--port', type=int, default=18850)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8850']
    try:
        roms = root / 'roms/noki8850'
        verify_inputs('8850', (roms / profile[1]).read_bytes(),
                      (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        for directory in ('cfg', 'nvram/nsm2hle', 'snap', 'sta'):
            (run / directory).mkdir(parents=True)
        host = args.scenario.startswith('host-')
        sms = args.scenario.endswith('-sms')
        incoming = args.scenario.startswith('host-incoming-')
        state = args.scenario.endswith('-state')
        script = {
            'registration': 'security_input',
            'host-incoming-call': 'host_incoming_input',
            'host-incoming-sms': 'incoming_sms_input',
            'host-outgoing-call': 'outgoing_call_input',
            'host-outgoing-sms': 'outgoing_sms_input',
            'phonebook': 'phonebook_input',
            'idle-state': 'state_idle', 'call-state': 'state_call', 'sms-state': 'state_sms',
        }[args.scenario]
        if host:
            shutil.copyfile(root / 'fixtures/noki8850_host/nsm2hle.cfg', run / 'cfg/nsm2hle.cfg')
        elif args.scenario == 'sms-state':
            shutil.copyfile(root / 'fixtures/noki8850_incoming_sms/nsm2hle.cfg', run / 'cfg/nsm2hle.cfg')
        card = run / 'nvram/nsm2hle/sim_card'
        card.write_bytes(make_profile(pin_enabled=not args.without_pin))
        environment = os.environ.copy()
        environment.pop('NOKIA_DCT3_8850_PIN_ENTRY', None)
        if not args.without_pin:
            environment['NOKIA_DCT3_8850_PIN_ENTRY'] = '1'
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsm2hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-state_directory', 'sta',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / f'tools/noki8850_{script}.lua'),
                   '-seconds_to_run', '60' if host or state else '46']
        host_command = None
        if host:
            command.extend(['-http', '-http_port', str(args.port)])
            runner, options = {
                'host-incoming-call': ('run_host_incoming_signaling_gate',
                    ['--caller', '5551234', '--ready-file', str(run / 'snap/8850_registered_idle.png')]),
                'host-incoming-sms': ('run_host_incoming_sms_gate', []),
                'host-outgoing-call': ('run_host_call_adapter_gate',
                    ['--number', '5551234', '--decision', 'connect']),
                'host-outgoing-sms': ('run_host_sms_gate',
                    ['--user-data', '41', '--user-data-length', '1']),
            }[args.scenario]
            host_command = [sys.executable, str(root / f'tools/{runner}.py'),
                            '--port', str(args.port), '--cwd', str(run)] + options
            host_command += ['--'] + command
        with (run / 'console.log').open('w') as console:
            subprocess.run(host_command or command, cwd=run, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
        text = (run / 'error.log').read_text(errors='replace')
        if not args.without_pin:
            validate(text, card.read_bytes(), 'verify', '1234')
            check_pin_inputs(text)
        errors = check_trace(text, runtime_hle=True, sim_reads=True)
        if args.scenario == 'registration':
            errors += check_physical_inputs(text) + check_navigation_frames(run / 'snap')
        if errors:
            raise ValueError('; '.join(errors))
        subprocess.run([sys.executable, str(root / 'tools/radio_registration_trace_check.py'),
                        str(run / 'error.log'), '--profile', 'nsm2'], check=True)
        if state:
            before_save = text.split('8850_state: event=saved', 1)[0]
            if args.scenario == 'call-state':
                from tools.radio_outgoing_call_trace_check import CONNECT_ACKNOWLEDGE
                if not CONNECT_ACKNOWLEDGE.search(before_save):
                    raise ValueError('call was not connected before saving')
            elif args.scenario == 'sms-state':
                if ('sim_device: update fid=6f3c record=1 length=176' not in before_save or
                        'LAPDm service Channel Release acknowledged' not in before_save):
                    raise ValueError('SMS was not stored and released before saving')
            options = {'idle-state': ['--idle'], 'call-state': [], 'sms-state': ['--sms', '--storage', str(card)]}[args.scenario]
            subprocess.run([sys.executable, str(root / 'tools/noki8850_state_check.py'),
                            str(run / 'error.log'), str(run / 'snap')] + options, check=True)
        elif args.scenario == 'phonebook':
            original_card = card.read_bytes()
            subprocess.run([sys.executable, str(root / 'tools/noki8850_phonebook_check.py'),
                            'save', str(run / 'error.log'), str(card),
                            str(run / 'snap/8850_phonebook_save.png')], check=True)
            shutil.copyfile(run / 'error.log', run / 'write.log')
            cold_command = command.copy()
            cold_command[cold_command.index('-autoboot_script') + 1] = str(
                root / 'tools/noki8850_phonebook_read.lua')
            with (run / 'cold_console.log').open('w') as console:
                subprocess.run(cold_command, cwd=run, env=environment, stdout=console,
                               stderr=subprocess.STDOUT, check=True, timeout=180)
            cold_text = (run / 'error.log').read_text(errors='replace')
            if card.read_bytes() != original_card:
                raise ValueError('cold phonebook read changed persistent SIM bytes')
            if not args.without_pin:
                validate(cold_text, card.read_bytes(), 'verify', '1234')
                check_pin_inputs(cold_text)
            cold_errors = check_trace(cold_text, runtime_hle=True, sim_reads=True)
            if cold_errors:
                raise ValueError('; '.join(cold_errors))
            subprocess.run([sys.executable, str(root / 'tools/radio_registration_trace_check.py'),
                            str(run / 'error.log'), '--profile', 'nsm2', '--preserved'], check=True)
            subprocess.run([sys.executable, str(root / 'tools/noki8850_phonebook_check.py'),
                            'readback', str(run / 'error.log'), str(card),
                            str(run / 'snap/8850_phonebook_contact.png')], check=True)
        elif sms and not incoming:
            from tools.noki8850_outgoing_sms_check import verify
            verify(text)
            subprocess.run([sys.executable, str(root / 'tools/radio_outgoing_host_sms_trace_check.py'),
                            '--octets', '1', str(run / 'error.log')], check=True)
        elif host and not incoming:
            from tools.noki8850_outgoing_call_check import verify, verify_frames
            verify(text)
            verify_frames(run / 'snap')
        elif sms:
            from tools.noki8850_sms_check import verify, verify_frame
            verify(text, card.read_bytes())
            verify_frame(run / 'snap/8850_sms_read_4.png')
            subprocess.run([sys.executable, str(root / 'tools/radio_incoming_host_sms_trace_check.py'),
                            str(run / 'error.log')], check=True)
        elif host:
            from tools.noki8850_incoming_call_check import verify, check_frames
            verify(text)
            check_frames(run / 'snap')
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm2hle',
            'sim_profile': 'default laboratory card' if args.without_pin else 'PIN-enabled laboratory card',
            'provisioning': 'own acquired PMM unchanged',
            'native_dsp_complete': False, 'speech_tested': False,
            'command': command, 'result': 'pass',
            'scenario': args.scenario, 'host_command': host_command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8850 PIN registration FAIL: {error}\n')
    print(f'8850 PIN registration PASS: {run}')


if __name__ == '__main__':
    main()
