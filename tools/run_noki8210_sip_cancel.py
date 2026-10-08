"""NSM-3 base-record comparison and real SIP signaling; no native speech."""
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
from tools.run_noki8210_acceptance import prepare_run, MCU_SHA1, PMM_SHA256
from tools.run_noki8890_host_sms import check_output
from tools.noki8210_staged_check import verify as verify_stage
from tools.noki8210_registration_check import verify as verify_registration
from tools.radio_call_lifecycle_common import INCOMING_SETUP

FRAMES = {
    '8210_sip_registered_idle.png': '1705ada895a4942757e7e237c74f9b88605456c381e2f41774664eedd8eb2b80',
    '8210_sip_missed_call.png': 'e8e8917c3b003a18326802259325704e0bf5e3106d1e782d740412b8ed2828c0',
    '8210_sip_after_cancel.png': '4f7c61b176efcf56d69c55c129e6b2a7f6ebcf65dad1d8443053f32da7d35df7',
}


def check_frames(directory):
    for name, expected in FRAMES.items():
        with Image.open(directory / name) as frame:
            # Ignore advancing clock pixels, retaining notification and softkeys.
            digest = hashlib.sha256(frame.convert('L').crop((0, 8, 84, 48)).tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != expected:
                raise ValueError('missing reviewed NSM-3 SIP cleanup frame: ' + name)


def check_product_result(run, restore_idle=False):
    text = (run / 'error.log').read_text(errors='replace')
    check_output(text)
    check_output((run / 'console.log').read_text(errors='replace'))
    verify_stage(text, runtime=True, selftest=True, base_record=True)
    if restore_idle:
        from tools.noki8210_state_check import verify as verify_state, check_frames as check_state_frames
        verify_state(text, sip_cancel=True)
        check_state_frames(run / 'snap', sip_cancel=True)
        restored = text.find('state_roundtrip: result=pass scenario=8210_idle')
        paging = text.find('gsm_call_adapter: incoming state id=1 epoch=')
        if restored < 0 or paging <= restored:
            raise ValueError('incoming SIP call did not follow exact idle restoration')
        if re.search(r'gsm_call_adapter: (?:incoming state id=|request id=)', text[:restored]):
            raise ValueError('external host call existed before idle restoration')
        if json.loads((run / 'sip-result.json').read_text()).get('epoch') != 2:
            raise ValueError('idle restore did not establish a fresh host epoch')
    verify_registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes(), configured_carrier=True)
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=4\b', text):
        raise ValueError('NSM-3 host registration lacks own laboratory carrier')
    if len(INCOMING_SETUP.findall(text)) != 1:
        raise ValueError('missing one SETUP for caller 5551234')
    cursor = 0
    for name, code in [('Keypad ' + str(key), f'{key:02x}') for key in range(1, 6)] + [('Menu', '19')]:
        match = re.search(r'8210_security_physical: key=' + re.escape(name) + r'\b', text[cursor:])
        if not match:
            raise ValueError('missing physical NSM-3 security input: ' + name)
        cursor += match.end()
        match = re.search(r'8210_keypad_decoded: key=' + code + r'\b', text[cursor:])
        if not match:
            raise ValueError('physical NSM-3 security input did not decode: ' + name)
        cursor += match.end()
    exit_at = text.find('8210_sip_cancel: physical Exit')
    if exit_at < cursor or not re.search(r'8210_keypad_decoded: key=1a\b', text[exit_at:]):
        raise ValueError('missed-call notification lacks decoded physical Exit')
    check_frames(run / 'snap')


def check_outgoing_result(run):
    from tools.noki8210_outgoing_call_check import FRAMES as outgoing_frames
    text = (run / 'error.log').read_text(errors='replace')
    check_output(text)
    check_output((run / 'console.log').read_text(errors='replace'))
    verify_stage(text, runtime=True, selftest=True, base_record=True)
    verify_registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes(), configured_carrier=True)
    if not re.search(r'gsm_call_adapter: network registered=1 arfcn=4\b', text):
        raise ValueError('NSM-3 host registration lacks own laboratory carrier')
    send_at = text.find('8210_call_physical: action=send')
    if send_at < 0 or not re.search(r'8210_keypad_decoded: key=0e\b', text[send_at:]):
        raise ValueError('outgoing call lacks decoded physical Send')
    for name in ('8210_dialed_number.png', '8210_after_outgoing_call.png'):
        crop, expected = outgoing_frames[name]
        with Image.open(run / 'snap' / name) as source:
            digest = hashlib.sha256(source.convert('L').crop(crop).tobytes()).hexdigest()
            if source.size != (84, 48) or digest != expected:
                raise ValueError('outgoing presentation differs: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--record-media', action='store_true',
                        help='capture outgoing HLE PCM and remote audio; does not assert non-silent waveform acceptance')
    parser.add_argument('--sound', choices=('none', 'pulse'), default='none')
    parser.add_argument('--restore-idle', action='store_true',
                        help='restore registered handset idle before admitting the fresh SIP call')
    outgoing = parser.add_mutually_exclusive_group()
    outgoing.add_argument('--incoming-media', action='store_true',
                          help='answer a fresh real SIP call using physical Send/End and validate HLE media transport')
    outgoing.add_argument('--outgoing-media', action='store_true',
                          help='physically dial against SIP 200 and validate HLE media transport, not native speech')
    outgoing.add_argument('--outgoing-busy', action='store_true',
                        help='physically dial 1234567 against a real SIP 486 response')
    outgoing.add_argument('--outgoing-unavailable', action='store_true',
                          help='physically dial 1234567 against a real SIP 480 response')
    parser.add_argument('--http-port', type=int, default=18821)
    parser.add_argument('--sip-port', type=int, default=25821)
    args = parser.parse_args()
    if args.record_media and not (args.outgoing_media or args.incoming_media):
        parser.error('--record-media requires a media fixture')
    if args.incoming_media and args.restore_idle:
        parser.error('incoming-media restoration is not validated')
    outgoing_call = args.outgoing_busy or args.outgoing_unavailable or args.outgoing_media
    if args.restore_idle and outgoing_call:
        parser.error('--restore-idle cannot be combined with an outgoing call')
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        prepare_run(run, (root / 'roms/noki8210/8210_5.31ppm_c.fls').read_bytes(),
                    (root / 'roms/noki8210/8210 virgin eeprom 003d0000.fls').read_bytes())
        shutil.copyfile(root / 'fixtures/noki8210_host_gsm900/nsm3hle.cfg', run / 'cfg/nsm3hle.cfg')
        handset = [str((args.mame or root / 'mame/mame').resolve()), 'nsm3hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', str(run / 'nvram'),
                   '-cfg_directory', str(run / 'cfg'), '-noreadconfig',
                   '-debug', '-debugger', 'none', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / (
                       'noki8210_outgoing_call_input.lua' if outgoing_call else
                       'noki8210_sip_answer_input.lua' if args.incoming_media else
                       'noki8210_sip_idle_restore.lua' if args.restore_idle else
                       'noki8210_sip_cancel_observe.lua')),
                   '-state_directory', str(run / 'sta'),
                   '-snapshot_directory', str(run / 'snap'), '-seconds_to_run',
                   '60' if args.restore_idle or args.incoming_media else '57',
                   '-video', 'none', '-sound', args.sound, '-throttle', '-log', '-verbose',
                   '-http', '-http_port', str(args.http_port)]
        command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                   '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                   '--product', '8210',
                   *(['--record-media'] if args.record_media else []),
                   *(['--sip-response', '200' if args.outgoing_media else '480' if args.outgoing_unavailable else '486'] if outgoing_call else
                     ['--incoming', *([] if args.incoming_media else ['--cancel-incoming']), '--ready-file',
                      str(run / 'snap' / ('8210_host_registered_idle.png' if args.incoming_media else
                                         '8210_sip_registered_idle.png'))]),
                   '--http-port', str(args.http_port), '--sip-port', str(args.sip_port),
                   '--', *handset]
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        if args.incoming_media:
            from tools.noki8210_incoming_call_check import verify, check_frames as incoming_frames
            text = (run / 'error.log').read_text(errors='replace')
            check_output(text)
            check_output((run / 'console.log').read_text(errors='replace'))
            verify_stage(text, runtime=True, selftest=True, base_record=True)
            verify_registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes(), configured_carrier=True)
            verify(text, configured_carrier=True)
            incoming_frames(run / 'snap')
        elif outgoing_call:
            check_outgoing_result(run)
        else:
            check_product_result(run, args.restore_idle)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nsm3hle', 'scenario': ('incoming-sip-hle-media' if args.incoming_media else
                                             'outgoing-sip-hle-media' if args.outgoing_media else
                                             'outgoing-sip-unavailable' if args.outgoing_unavailable else
                                             'outgoing-sip-busy' if args.outgoing_busy else 'incoming-sip-cancel'),
            'mcu_sha1': MCU_SHA1, 'acquired_pmm_sha256': PMM_SHA256,
            'provisioning': 'unchanged acquired base record; later low journal omitted',
            'native_dsp_complete': False, 'speech_tested': False,
            'hle_media_transport_tested': args.outgoing_media or args.incoming_media,
            'media_recorded': args.record_media,
            'factory_provisioning_validated': False,
            'laboratory_carrier': 4,
            'idle_restored': args.restore_idle,
            'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'8210 SIP signaling FAIL: {error}; inspect {run}\n')
    print('8210 real SIP ' + ('HLE media transport' if args.outgoing_media or args.incoming_media else 'signaling') +
          ' PASS; base-record comparison, no native or non-silent speech acceptance')


if __name__ == '__main__':
    main()
