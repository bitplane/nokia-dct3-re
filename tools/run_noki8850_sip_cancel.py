"""Own-PMM NSM-2 real SIP signaling and explicit outgoing HLE media."""
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


def check_product_result(run, restore_idle=False):
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
    if restore_idle:
        from tools.noki8850_state_check import verify as verify_state, check_frames as check_state_frames
        verify_state(text, idle=True, sip_cancel=True)
        check_state_frames(run / 'snap', idle=True, sip_cancel=True)
        restored = text.find('state_roundtrip: result=pass scenario=8850_idle')
        paging = text.find('gsm_call_adapter: incoming state id=1 epoch=')
        if restored < 0 or paging <= restored:
            raise ValueError('incoming SIP call did not follow exact idle restoration')
        if json.loads((run / 'sip-result.json').read_text()).get('epoch') != 2:
            raise ValueError('idle restore did not establish a fresh host epoch')
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


def check_outgoing_result(run):
    from tools.noki8850_outgoing_call_check import verify_frames
    text = (run / 'error.log').read_text(errors='replace')
    check_output(text)
    check_output((run / 'console.log').read_text(errors='replace'))
    errors = check_trace(text, runtime_hle=True)
    if errors:
        raise ValueError('; '.join(errors))
    verify_registration(text, 'nsm2')
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=1\b', text):
        raise ValueError('host registration lacks own laboratory carrier')
    send_at = text.find('8850_call_physical: action=send')
    if send_at < 0 or not re.search(r'8850_keypad_decoded key=0e\b', text[send_at:]):
        raise ValueError('outgoing call lacks decoded physical Send')
    verify_frames(run / 'snap')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--restore-idle', action='store_true',
                        help='restore idle before admitting the fresh unanswered SIP call')
    outgoing = parser.add_mutually_exclusive_group()
    outgoing.add_argument('--outgoing-busy', action='store_true',
                        help='exercise physical dialing against a real SIP 486 response')
    outgoing.add_argument('--outgoing-unavailable', action='store_true',
                          help='exercise physical dialing against a real SIP 480 response')
    outgoing.add_argument('--outgoing-media', action='store_true',
                          help='verify physical outgoing SIP connection and HLE frame transport')
    parser.add_argument('--record-media', action='store_true')
    parser.add_argument('--sound', choices=('none', 'pulse'), default='none')
    parser.add_argument('--http-port', type=int, default=18885)
    parser.add_argument('--sip-port', type=int, default=25885)
    args = parser.parse_args()
    outgoing_failure = args.outgoing_busy or args.outgoing_unavailable
    if args.record_media and not args.outgoing_media:
        parser.error('--record-media requires --outgoing-media')
    if args.restore_idle and (outgoing_failure or args.outgoing_media):
        parser.error('--restore-idle cannot be combined with outgoing modes')
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
                   '-autoboot_script', str(root / 'tools' / (
                       'noki8850_outgoing_call_input.lua' if outgoing_failure or args.outgoing_media else
                       'noki8850_sip_idle_restore.lua' if args.restore_idle else
                       'noki8850_sip_cancel_observe.lua')),
                   '-state_directory', str(run / 'sta'),
                   '-snapshot_directory', str(run / 'snap'), '-seconds_to_run',
                   '60' if args.restore_idle else '57',
                   '-video', 'none', '-sound', args.sound, '-throttle', '-log', '-verbose',
                   '-http', '-http_port', str(args.http_port)]
        command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                   '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                   '--product', '8850',
                   *(['--record-media'] if args.record_media else []),
                   *(['--sip-response', '480' if args.outgoing_unavailable else '486'] if outgoing_failure else
                     [] if args.outgoing_media else
                     ['--incoming', '--cancel-incoming', '--ready-file',
                      str(run / 'snap/8850_sip_registered_idle.png')]),
                   '--http-port', str(args.http_port), '--sip-port', str(args.sip_port),
                   '--', *handset]
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        if outgoing_failure or args.outgoing_media:
            check_outgoing_result(run)
            if args.outgoing_media:
                from tools.noki8850_speech_control_check import recover, verify_pcm
                recover((roms / profile[1]).read_bytes())
                verify_pcm((run / 'error.log').read_text(errors='replace'))
        else:
            check_product_result(run, args.restore_idle)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm2hle', 'scenario': ('outgoing-sip-media' if args.outgoing_media else
                                             'outgoing-sip-unavailable' if args.outgoing_unavailable else
                                             'outgoing-sip-busy' if args.outgoing_busy else 'incoming-sip-cancel'),
            'mcu_sha1': profile[2], 'pmm_sha1': profile[4],
            'provisioning': 'own acquired PMM unchanged', 'laboratory_carrier': 1,
            'native_dsp_complete': False, 'speech_tested': False,
            'hle_media_transport_tested': args.outgoing_media,
            'waveform_tested': False,
            'media_recorded': args.record_media,
            'idle_restored': args.restore_idle,
            'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8850 SIP signaling FAIL: {error}; inspect {run}\n')
    print('8850 real SIP ' + ('HLE media transport' if args.outgoing_media else 'signaling') +
          ' PASS; no native speech or waveform acceptance')


if __name__ == '__main__':
    main()
