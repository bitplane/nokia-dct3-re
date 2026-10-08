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
    'ussd': ('ussd_input', 40, 'ussd_check'),
    'registration': ('registration_input', 46, 'registration_check'),
    'outgoing-call': ('outgoing_call_input', 49, 'outgoing_call_check'),
    'incoming-call': ('incoming_call_input', 44, 'incoming_call_check'),
    'incoming-sms': ('incoming_sms_input', 32, 'incoming_sms_check'),
    'outgoing-sms': ('outgoing_sms_input', 46, 'outgoing_sms_check'),
    'calculator': ('calculator_input', 41, None),
    'phonebook': ('phonebook_input', 34, 'phonebook_check'),
    'idle-state': ('state_idle', 40, 'state_check'),
    'call-state': ('state_call', 50, 'state_check'),
    'sms-state': ('state_sms', 32, 'state_check'),
    'host-incoming-call': ('host_incoming_input', 60, 'incoming_call_check'),
    'host-incoming-sms': ('incoming_sms_input', 32, 'incoming_sms_check'),
    'host-outgoing-sms': ('outgoing_sms_input', 46, 'outgoing_sms_check'),
    'host-rejected-sms': ('sms_reject_input', 58, 'outgoing_sms_check'),
    'host-silent-sms': ('sms_silence_input', 125, 'outgoing_sms_check'),
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
    parser.add_argument('--port', type=int, default=18991)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        mcu = (root / 'roms/noki8210/8210_5.31ppm_c.fls').read_bytes()
        pmm = (root / 'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes()
        if args.scenario == 'ussd':
            from tools.noki8210_ussd_check import verify_key_table
            verify_key_table(mcu)
        prepare_run(run, mcu, pmm)
        config = 'noki8210_host' if args.scenario.startswith('host-') else {
                  'incoming-call': 'radio_incoming_call_answered',
                  'incoming-sms': 'radio_incoming_sms',
                  'sms-state': 'radio_incoming_sms'}.get(args.scenario)
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
        host_command = None
        if args.scenario.startswith('host-'):
            command.extend(['-http', '-http_port', str(args.port)])
            runner, options = {
                'host-incoming-call': ('run_host_incoming_signaling_gate', [
                    '--caller', '5551234', '--ready-file',
                    str(run / 'snap/8210_host_registered_idle.png')]),
                'host-incoming-sms': ('run_host_incoming_sms_gate', []),
                'host-outgoing-sms': ('run_host_sms_gate', [
                    '--user-data', '41', '--user-data-length', '1']),
                'host-rejected-sms': ('run_host_sms_gate', [
                    '--user-data', '41', '--user-data-length', '1', '--decision', 'rp_error']),
                'host-silent-sms': ('run_host_sms_gate', [
                    '--user-data', '41', '--user-data-length', '1', '--decision', 'rp_silence']),
            }[args.scenario]
            host_command = [sys.executable, str(root / f'tools/{runner}.py'),
                            '--port', str(args.port), '--cwd', str(run)] + options + ['--'] + command
            execute(host_command, 'console.log')
        else:
            execute(command, 'console.log')
        check = [sys.executable, str(root / f'tools/noki8210_{checker}.py'), str(run / 'error.log')]
        storage = str(run / 'nvram/nsm3hle/sim_card')
        if args.scenario == 'host-incoming-call':
            check.extend(['--frames', str(run / 'snap')])
        elif args.scenario in ('host-rejected-sms', 'host-silent-sms'):
            check.extend(['--rejected' if args.scenario == 'host-rejected-sms' else '--rp-silence',
                          '--recovery-frames', str(run / 'snap')])
        elif args.scenario == 'phonebook':
            shutil.copyfile(run / 'error.log', run / 'write.log')
            cold = command.copy()
            cold[cold.index('-autoboot_script') + 1] = str(root / 'tools/noki8210_phonebook_read.lua')
            cold[cold.index('-seconds_to_run') + 1] = '28'
            execute(cold, 'cold_console.log')
            check = [sys.executable, str(root / 'tools/noki8210_phonebook_check.py'),
                     str(run / 'write.log'), str(run / 'error.log'), storage]
        elif args.scenario in ('idle-state', 'call-state', 'sms-state'):
            check.append(str(run / 'snap'))
            if args.scenario == 'call-state':
                check.append('--call')
            elif args.scenario == 'sms-state':
                check.extend(['--sms', '--storage', storage])
        elif args.scenario in ('registration', 'incoming-sms', 'host-incoming-sms'):
            check.append(storage)
            if args.scenario == 'host-incoming-sms':
                check.extend(['--frame', str(run / 'snap/8210_sms_read_2.png')])
        if checker:
            subprocess.run(check, cwd=root, check=True)
            if args.scenario in ('host-incoming-sms', 'host-outgoing-sms'):
                incoming = args.scenario == 'host-incoming-sms'
                host_checker = 'radio_incoming_host_sms_trace_check' if incoming else 'radio_outgoing_host_sms_trace_check'
                subprocess.run([sys.executable, str(root / f'tools/{host_checker}.py')] +
                               ([] if incoming else ['--octets', '1']) + [str(run / 'error.log')],
                               cwd=root, check=True)
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
            'host_command': host_command,
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'8210 acceptance FAIL: {error}\n')
    print(f'8210 {args.scenario} acceptance PASS: {run}')


CALCULATOR_SHA256 = '04ff410190e8c72f1fa6dbbb00c965051c520fee4b7ca6fe9c7d395ccdfc23e5'

if __name__ == '__main__':
    main()
