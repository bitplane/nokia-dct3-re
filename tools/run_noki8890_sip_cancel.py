"""Own-PMM NSB-6 real SIP cancellation; no Answer, media or native DSP claim."""
import argparse
import json
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.run_noki8890_host_sms import check_output
from tools.noki8890_staged_check import verify as verify_stage
from tools.noki8890_incoming_call_check import host_setup_pattern
from PIL import Image

FRAMES = {
    '8890_sip_registered_idle.png': '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de',
    '8890_sip_missed_call.png': 'e367f48115cd4ba19751cb3e813ac7873d45157552b943a97667e5f9be101e8f',
    '8890_sip_after_cancel.png': '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de',
}


def check_frames(directory):
    for name, expected in FRAMES.items():
        with Image.open(directory / name) as source:
            # The top-row clock advances; preserve notification, softkeys and idle content.
            digest = hashlib.sha256(source.convert('L').crop((0, 8, 84, 48)).tobytes()).hexdigest()
            if source.size != (84, 48) or digest != expected:
                raise ValueError('missing reviewed SIP cleanup frame: ' + name)


def check_product_result(run):
    text = (run / 'error.log').read_text(errors='replace')
    check_output(text)
    check_output((run / 'console.log').read_text(errors='replace'))
    verify_stage(text, runtime=True, selftest=True)
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=60\b', text):
        raise ValueError('host registration lacks own configured carrier')
    if len(host_setup_pattern('5551234').findall(text)) != 1:
        raise ValueError('missing one own SETUP for the real SIP caller')
    storage = (run / 'nvram/nsb6hle/sim_card').read_bytes()
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted SIM location is not laboratory-registered')
    exit_at = text.find('8890_sip_cancel: physical Exit')
    if exit_at < 0 or not re.search(r'8890_keypad_decoded: key=1a\b', text[exit_at:]):
        raise ValueError('missed-call notification was not dismissed by decoded physical Exit')
    check_frames(run / 'snap')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--http-port', type=int, default=18889)
    parser.add_argument('--sip-port', type=int, default=25889)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8890']
    try:
        roms = root / 'roms/noki8890'
        verify_inputs('8890', (roms / profile[1]).read_bytes(), (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        (run / 'cfg').mkdir()
        shutil.copyfile(root / 'fixtures/noki8890_host/nsb6hle.cfg', run / 'cfg/nsb6hle.cfg')
        handset = [str((args.mame or root / 'mame/mame').resolve()), 'nsb6hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', str(run / 'nvram'),
                   '-cfg_directory', str(run / 'cfg'), '-noreadconfig',
                   '-debug', '-debugger', 'none', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki8890_sip_cancel_observe.lua'),
                   '-snapshot_directory', str(run / 'snap'), '-seconds_to_run', '70',
                   '-video', 'none', '-sound', 'none', '-throttle', '-log', '-verbose',
                   '-http', '-http_port', str(args.http_port)]
        command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                   '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                   '--product', '8890', '--incoming', '--cancel-incoming',
                   '--ready-file', str(run / 'snap/8890_sip_registered_idle.png'),
                   '--http-port', str(args.http_port), '--sip-port', str(args.sip_port),
                   '--', *handset]
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        check_product_result(run)
        subprocess.run([sys.executable, str(root / 'tools/noki8890_registration_check.py'),
                        '--configured-gsm900', str(run / 'error.log')], check=True)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsb6hle', 'scenario': 'incoming-sip-cancel',
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'provisioning': 'own acquired PMM unchanged', 'laboratory_carrier': 60,
            'native_dsp_complete': False, 'speech_tested': False,
            'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8890 SIP cancellation FAIL: {error}; inspect {run}\n')
    print('8890 real SIP CANCEL signaling PASS; no speech acceptance')


if __name__ == '__main__':
    main()
