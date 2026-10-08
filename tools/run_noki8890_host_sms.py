"""Isolated own-PMM 8890 physical SMS through the software host adapter."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs


def check_output(text):
    for error in ('[LUA ERROR]', 'Disk quota exceeded', 'Error writing NVRAM file',
                  'Error generating PNG'):
        if error in text:
            raise ValueError('host SMS acceptance artifacts failed: ' + error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--direction', choices=('incoming', 'outgoing'), required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--port', type=int, default=18991)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8890']
    incoming = args.direction == 'incoming'
    try:
        roms = root / 'roms/noki8890'
        verify_inputs('8890', (roms / profile[1]).read_bytes(), (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        for directory in ('cfg', 'nvram', 'snap'):
            (run / directory).mkdir()
        shutil.copyfile(root / 'fixtures/noki8890_host/nsb6hle.cfg', run / 'cfg/nsb6hle.cfg')
        command = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', 'nvram',
                   '-cfg_directory', 'cfg', '-snapshot_directory', 'snap', '-noreadconfig',
                   '-debug', '-debugger', 'none', '-verbose', '-log', '-video', 'none',
                   '-sound', 'none', '-nothrottle', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / f'tools/noki8890_{args.direction}_sms_input.lua'),
                   '-seconds_to_run', '32' if incoming else '48',
                   '-http', '-http_port', str(args.port)]
        host_script = 'run_host_incoming_sms_gate' if incoming else 'run_host_sms_gate'
        host_command = [sys.executable, str(root / f'tools/{host_script}.py'),
                        '--port', str(args.port), '--cwd', str(run)]
        if not incoming:
            host_command.extend(['--user-data', '41', '--user-data-length', '1'])
        host_command.extend(['--'] + command)
        with (run / 'console.log').open('w') as console:
            subprocess.run(host_command, stdout=console, stderr=subprocess.STDOUT, check=True)
        check_output((run / 'console.log').read_text(errors='replace'))
        log = str(run / 'error.log')
        check_output(Path(log).read_text(errors='replace'))
        subprocess.run([sys.executable, str(root / 'tools/noki8890_registration_check.py'),
                        '--configured-gsm900', log], check=True)
        check = [sys.executable, str(root / f'tools/noki8890_{args.direction}_sms_check.py'), log]
        if incoming:
            check.extend([str(run / 'nvram/nsb6hle/sim_card'), str(run / 'snap/8890_sms_read_2.png')])
        subprocess.run(check, check=True)
        host_check = 'radio_incoming_host_sms_trace_check' if incoming else 'radio_outgoing_host_sms_trace_check'
        subprocess.run([sys.executable, str(root / f'tools/{host_check}.py')] +
                       (['--arfcn', '60'] if incoming else ['--octets', '1']) + [log], check=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsb6hle', 'scenario': 'host-' + args.direction + '-sms',
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'provisioning': 'own acquired PMM unchanged',
            'native_dsp_complete': False, 'speech_tested': False,
            'laboratory_carrier': 60,
            'command': command, 'host_command': host_command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'8890 host SMS FAIL: {error}\n')
    print(f'8890 host {args.direction} SMS acceptance PASS: {run}')


if __name__ == '__main__':
    main()
