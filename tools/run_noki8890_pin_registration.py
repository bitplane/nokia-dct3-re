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
    parser.add_argument('--scenario', choices=('registration', 'host-incoming-call', 'host-incoming-sms',
                                              'host-outgoing-call', 'host-outgoing-sms'), default='registration')
    parser.add_argument('--port', type=int, default=18890)
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
        script = {'registration': 'security_input', 'host-incoming-call': 'clock_incoming_input',
                  'host-incoming-sms': 'incoming_sms_input', 'host-outgoing-call': 'clock_call_input',
                  'host-outgoing-sms': 'outgoing_sms_input'}[args.scenario]
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / f'tools/noki8890_{script}.lua'),
                   '-seconds_to_run', '74' if args.scenario.endswith('-call') else
                       '60' if args.scenario.endswith('-sms') else '46']
        host_command = None
        if args.scenario == 'host-incoming-call':
            command += ['-http', '-http_port', str(args.port)]
            host_command = [sys.executable, str(root / 'tools/run_host_incoming_signaling_gate.py'),
                            '--port', str(args.port), '--cwd', str(run), '--caller', '447700900123',
                            '--ready-file', str(run / 'snap/8890_date_after.png'), '--'] + command
        elif args.scenario == 'host-incoming-sms':
            command += ['-http', '-http_port', str(args.port)]
            host_command = [sys.executable, str(root / 'tools/run_host_incoming_sms_gate.py'),
                            '--port', str(args.port), '--cwd', str(run), '--'] + command
        elif args.scenario.startswith('host-outgoing-'):
            command += ['-http', '-http_port', str(args.port)]
            call = args.scenario == 'host-outgoing-call'
            runner = 'run_host_call_adapter_gate' if call else 'run_host_sms_gate'
            options = ['--number', '1234567', '--decision', 'connect'] if call else [
                '--user-data', '41', '--user-data-length', '1']
            host_command = [sys.executable, str(root / f'tools/{runner}.py'),
                            '--port', str(args.port), '--cwd', str(run)] + options + ['--'] + command
        with (run / 'console.log').open('w') as console:
            subprocess.run(host_command or command, cwd=run, env=environment, stdout=console,
                           stderr=subprocess.STDOUT, check=True, timeout=180)
        text = (run / 'error.log').read_text(errors='replace')
        if not args.without_pin:
            validate(text, card.read_bytes(), 'verify', '1234')
            check_pin_inputs(text)
        verify_stage(text, runtime=True, selftest=True)
        verify_registration(text, configured_gsm900=True)
        if args.scenario == 'host-incoming-call':
            from tools.noki8890_incoming_call_check import verify, check_host_frames
            verify(text, caller='447700900123', configured_gsm900=True)
            check_host_frames(run / 'snap', '447700900123')
        elif args.scenario == 'host-incoming-sms':
            subprocess.run([sys.executable, str(root / 'tools/noki8890_incoming_sms_check.py'),
                            str(run / 'error.log'), str(card),
                            str(run / 'snap/8890_sms_read_2.png')], check=True)
            subprocess.run([sys.executable, str(root / 'tools/radio_incoming_host_sms_trace_check.py'),
                            '--arfcn', '60', str(run / 'error.log')], check=True)
        elif args.scenario == 'host-outgoing-call':
            from tools.noki8890_outgoing_call_check import verify
            from tools.noki8890_clock_check import verify as verify_clock, check_frames
            verify(text, configured_gsm900=True)
            verify_clock(text)
            check_frames(run / 'snap', call=True)
        elif args.scenario == 'host-outgoing-sms':
            subprocess.run([sys.executable, str(root / 'tools/noki8890_outgoing_sms_check.py'),
                            str(run / 'error.log')], check=True)
            subprocess.run([sys.executable, str(root / 'tools/radio_outgoing_host_sms_trace_check.py'),
                            '--octets', '1', str(run / 'error.log')], check=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsb6hle', 'sim_pin_enabled': not args.without_pin,
            'provisioning': 'own acquired PMM unchanged', 'command': command,
            'native_dsp_complete': False, 'speech_tested': False, 'result': 'pass',
            'scenario': args.scenario, 'host_command': host_command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 PIN registration FAIL: {error}\n')
    print(f'8890 PIN registration PASS: {run}')


if __name__ == '__main__':
    main()
