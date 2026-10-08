"""Own-PMM NSM-2 real SIP cancellation; no Answer or native speech claim."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki8xxx_supplementary import PROFILES, verify_inputs
from tools.run_noki8890_host_sms import check_output
from tools.noki8850_staged_trace_check import check_trace, check_correlated_inputs
from tools.radio_registration_trace_check import verify as verify_registration
from tools.noki8850_incoming_call_check import INCOMING_SETUP

FRAMES = {
    '8850_sip_registered_idle.png': '1705ada895a4942757e7e237c74f9b88605456c381e2f41774664eedd8eb2b80',
    '8850_sip_missed_call.png': 'e8e8917c3b003a18326802259325704e0bf5e3106d1e782d740412b8ed2828c0',
    '8850_sip_after_cancel.png': '4f7c61b176efcf56d69c55c129e6b2a7f6ebcf65dad1d8443053f32da7d35df7',
}


def check_frames(directory):
    for name, expected in FRAMES.items():
        with Image.open(directory / name) as frame:
            # Exclude top-row clock, but retain notification and both softkeys.
            digest = hashlib.sha256(frame.convert('L').crop((0, 8, 84, 48)).tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != expected:
                raise ValueError('missing reviewed SIP cleanup frame: ' + name)


def check_product_result(run):
    text = (run / 'error.log').read_text(errors='replace')
    check_output(text)
    check_output((run / 'console.log').read_text(errors='replace'))
    errors = check_trace(text, runtime_hle=True)
    errors += check_correlated_inputs(text, tuple(
        ('security_physical: key=' + key, decoded) for key, decoded in (
            ('Keypad 1', '01'), ('Keypad 2', '02'), ('Keypad 3', '03'),
            ('Keypad 4', '04'), ('Keypad 5', '05'), ('Menu', '19'))))
    if errors:
        raise ValueError('; '.join(errors))
    verify_registration(text, 'nsm2')
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=1\b', text):
        raise ValueError('host registration lacks own laboratory carrier')
    if len(INCOMING_SETUP.findall(text)) != 1:
        raise ValueError('missing one own SETUP for caller 5551234')
    storage = (run / 'nvram/nsm2hle/sim_card').read_bytes()
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted SIM location is not laboratory-registered')
    exit_at = text.find('8850_sip_cancel: physical Exit')
    if exit_at < 0 or not re.search(r'8850_keypad_decoded key=1a\b', text[exit_at:]):
        raise ValueError('missed-call notification lacks decoded physical Exit')
    check_frames(run / 'snap')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--http-port', type=int, default=18885)
    parser.add_argument('--sip-port', type=int, default=25885)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    profile = PROFILES['8850']
    try:
        roms = root / 'roms/noki8850'
        verify_inputs('8850', (roms / profile[1]).read_bytes(), (roms / profile[3]).read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        (run / 'cfg').mkdir()
        shutil.copyfile(root / 'fixtures/noki8850_host/nsm2hle.cfg', run / 'cfg/nsm2hle.cfg')
        handset = [str((args.mame or root / 'mame/mame').resolve()), 'nsm2hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', str(run / 'nvram'),
                   '-cfg_directory', str(run / 'cfg'), '-noreadconfig',
                   '-debug', '-debugger', 'none', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools/noki8850_sip_cancel_observe.lua'),
                   '-snapshot_directory', str(run / 'snap'), '-seconds_to_run', '57',
                   '-video', 'none', '-sound', 'none', '-throttle', '-log', '-verbose',
                   '-http', '-http_port', str(args.http_port)]
        command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                   '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                   '--product', '8850', '--incoming', '--cancel-incoming',
                   '--ready-file', str(run / 'snap/8850_sip_registered_idle.png'),
                   '--http-port', str(args.http_port), '--sip-port', str(args.sip_port),
                   '--', *handset]
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        check_product_result(run)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm2hle', 'scenario': 'incoming-sip-cancel',
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'provisioning': 'own acquired PMM unchanged', 'laboratory_carrier': 1,
            'native_dsp_complete': False, 'speech_tested': False,
            'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8850 SIP cancellation FAIL: {error}; inspect {run}\n')
    print('8850 real SIP CANCEL signaling PASS; no speech acceptance')


if __name__ == '__main__':
    main()
