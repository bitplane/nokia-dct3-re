"""NHM-3 initial-record comparison and real SIP CANCEL/failures; no speech."""
import argparse
import os
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import re
from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.run_noki6250_acceptance import prepare_run, apply_coherent_config
from tools.noki6250_coherent_registration_check import verify as check_registration
from tools.run_noki8890_host_sms import check_output
from tools.radio_call_lifecycle_common import INCOMING_SETUP
from tools.radio_call_lifecycle_common import require_ordered, IDLE_PCH


FRAMES = {
    '6250_sip_registered_idle.png': '519c59eb68967255c5b2f7cd2b8ae2351cac00b03828c78cf86109e5496a6f49',
    '6250_sip_missed_call.png': 'e14c6bafddb6113310a53f04ab9eed869d35350d79004fbd42340779be7fbb98',
    '6250_sip_after_cancel.png': '519c59eb68967255c5b2f7cd2b8ae2351cac00b03828c78cf86109e5496a6f49',
}


def check_restored_dialog(result):
    if result.get('passed') is not True or result.get('sip_status') != 487 or result.get('epoch') != 2:
        raise ValueError('NHM-3 restored idle requires a fresh epoch-2 CANCEL/487 dialog')
    media = result.get('media', {})
    if any(media.get(name) != 0 for name in (
            'uplink', 'downlink', 'pcm_transmitted', 'pcm_received', 'dropped')):
        raise ValueError('NHM-3 restored unanswered dialog produced media')


def check_frames(directory):
    for name, expected in FRAMES.items():
        with Image.open(directory / name) as frame:
            # Exclude advancing clock pixels, retain notification and softkeys.
            if frame.size != (96, 60) or hashlib.sha256(
                    frame.convert('L').crop((0, 8, 96, 60)).tobytes()).hexdigest() != expected:
                raise ValueError('NHM-3 reviewed SIP cleanup pixels differ: ' + name)


def check_alerting_restoration(text):
    states = re.findall(r'6250_state: event=(saved|restored) pc=(\w+) sp=(\w+) ram=(\w+) t=([0-9.]+)', text)
    if (len(states) != 2 or [state[0] for state in states] != ['saved', 'restored'] or
            states[0][1:] != states[1][1:] or '6250_state: FAIL' in text):
        raise ValueError('NHM-3 ringing architecture did not restore exactly')
    cursor = 0
    for marker in ('incoming state id=1 epoch=1 phase=alerting', 'sip_state: saved',
                   'sip_state: restored', 'state_roundtrip: result=pass scenario=6250_incoming_alerting',
                   '6250_sip_cancel: physical Exit'):
        position = text.find(marker, cursor)
        if position < 0:
            raise ValueError('missing ordered NHM-3 restoration checkpoint: ' + marker)
        cursor = position + len(marker)


def check_product_result(run, *, restore_idle=False, restore_alerting=False):
    text = (run / 'error.log').read_text(errors='replace')
    check_output(text)
    check_output((run / 'console.log').read_text(errors='replace'))
    check_registration(text, (run / 'nvram/nhm3hle/sim_card').read_bytes())
    if restore_alerting:
        check_alerting_restoration(text)
    if restore_idle:
        from tools.noki6250_state_check import verify, check_frames as check_state_frames
        verify(text, fresh_sip=True)
        check_state_frames(run / 'snap', fresh_sip=True)
        check_restored_dialog(json.loads((run / 'sip-result.json').read_text()))
    if len(INCOMING_SETUP.findall(text)) != 1:
        raise ValueError('NHM-3 lacks exactly one SETUP for caller 5551234')
    if re.search(r'GSM service uplink sapi=0 pd=03 message=07\b|'
                 r'gsm_call_adapter: incoming state .*phase=connected', text):
        raise ValueError('NHM-3 cancelled call was answered')
    require_ordered(text, (
        ('own release carrier 19', re.compile(
            r'TX packet type=02 payload=20 .*radio_phase=release_channel_change '
            r'data=041202001117001a600000130000001400000001')),
        ('own release confirmation', re.compile(
            r'6250_channel_confirmation: body=00 input=0409 expected=00 pending=00')),
        ('resumed idle paging', IDLE_PCH),
        ('physical Exit', re.compile(r'6250_sip_cancel: physical Exit')),
        ('own decoded Exit', re.compile(r'6250_raw_matrix_key: value=15\b')),
    ), 'NHM-3 SIP cancellation')
    check_frames(run / 'snap')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--http-port', type=int, default=18625)
    parser.add_argument('--sip-port', type=int, default=25625)
    parser.add_argument('--restore-idle', action='store_true')
    outgoing = parser.add_mutually_exclusive_group()
    outgoing.add_argument('--outgoing-busy', action='store_true')
    outgoing.add_argument('--outgoing-unavailable', action='store_true')
    outgoing.add_argument('--restore-incoming-alerting', action='store_true')
    args = parser.parse_args()
    outgoing_status = 486 if args.outgoing_busy else 480 if args.outgoing_unavailable else None
    if (outgoing_status or args.restore_incoming_alerting) and args.restore_idle:
        parser.error('outgoing failures cannot use the incoming idle-restore fixture')
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        accessory, audit = prepare_run(run, root)
        apply_coherent_config(run / 'cfg/nhm3hle.cfg', root / 'fixtures/noki6250_host_gsm900/nhm3hle.cfg')
        script = ('noki6250_sip_incoming_alerting_restore.lua' if args.restore_incoming_alerting else
                  'noki6250_call_observe.lua' if outgoing_status else
                  'noki6250_sip_idle_restore.lua' if args.restore_idle else 'noki6250_sip_cancel_observe.lua')
        handset = [str((args.mame or root / 'mame/mame').resolve()), 'nhm3hle',
                   '-rompath', f"{run / 'roms'};{root / 'roms'}", '-nvram_directory', str(run / 'nvram'),
                   '-cfg_directory', str(run / 'cfg'), '-noreadconfig', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / script),
                   '-snapshot_directory', str(run / 'snap'), '-seconds_to_run', '57',
                   '-state_directory', str(run / 'sta'),
                   '-video', 'none', '-sound', 'none', '-throttle', '-log', '-verbose',
                   '-http', '-http_port', str(args.http_port)]
        command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                   '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                   '--product', '6250',
                   *(['--sip-response', str(outgoing_status)] if outgoing_status else
                     ['--incoming', '--restore-call', '--restore-phase', 'alerting', '--ready-file',
                      str(run / 'snap/6250_sip_registered_idle.png')] if args.restore_incoming_alerting else
                     ['--incoming', '--cancel-incoming', '--ready-file',
                      str(run / 'snap/6250_sip_registered_idle.png')]),
                   '--http-port', str(args.http_port), '--sip-port', str(args.sip_port), '--', *handset]
        with (run / 'console.log').open('w') as output:
            environment = dict(os.environ)
            if outgoing_status:
                environment['NOKIA_DCT3_6250_OUTGOING'] = '1'
            subprocess.run(command, cwd=run, env=environment, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        if outgoing_status:
            text = (run / 'error.log').read_text(errors='replace')
            check_output(text)
            check_output((run / 'console.log').read_text(errors='replace'))
            check_registration(text, (run / 'nvram/nhm3hle/sim_card').read_bytes())
            require_ordered(text, (
                ('physical Send', re.compile(r'6250_call_input: step=7 pressed=1')),
                *((('own traffic carrier 19', re.compile(
                    r'TX packet type=02 payload=20 .*radio_phase=traffic_channel_change '
                    r'data=041202000271012fc10000130000000400000000')),
                   ('cause-18 termination', re.compile(
                    r'outgoing termination consumed id=1 cause=18\b')))
                  if outgoing_status == 480 else ()),
                ('own release carrier 19', re.compile(
                    r'TX packet type=02 payload=20 .*radio_phase=release_channel_change '
                    + (r'data=041202001117001a600000130000001400000001' if outgoing_status == 480 else
                       r'data=041202000000001a600000130000000f00000000'))),
                ('own release confirmation', re.compile(
                    r'6250_channel_confirmation: body=00 input=0409 expected='
                    + ('00' if outgoing_status == 480 else '01') + r' pending=00')),
                ('resumed idle paging', IDLE_PCH),
            ), 'NHM-3 outgoing SIP failure')
            with Image.open(run / 'snap/6250_call_4.png') as frame:
                if frame.size != (96, 60) or hashlib.sha256(
                        frame.convert('L').crop((0, 8, 96, 60)).tobytes()).hexdigest() != FRAMES['6250_sip_after_cancel.png']:
                    raise ValueError('NHM-3 registered idle after failure differs')
        else:
            check_product_result(run, restore_idle=args.restore_idle,
                                 restore_alerting=args.restore_incoming_alerting)
        (run / 'acceptance.json').write_text(json.dumps({
            'machine': 'nhm3hle',
            'scenario': ('incoming-sip-alerting-restore' if args.restore_incoming_alerting else
                         'outgoing-sip-busy' if outgoing_status == 486 else
                         'outgoing-sip-unavailable' if outgoing_status else 'incoming-sip-cancel'),
            'provisioning': 'derived acquired initial-record PMM comparison',
            'shared_rom_audit_members': audit, 'accessory_contract': accessory,
            'native_dsp_complete': False, 'speech_tested': False,
            'idle_restored_before_fresh_sip': args.restore_idle,
            'incoming_alerting_restored': args.restore_incoming_alerting,
            'external_dialog_restored': False,
            'laboratory_carrier': 19, 'command': command, 'result': 'pass',
        }, indent=2) + '\n')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6250 SIP signaling FAIL: {error}; inspect {run}\n')
    print('6250 real SIP ' + (f'outgoing {outgoing_status}' if outgoing_status else 'CANCEL') +
          ' signaling PASS; initial-record comparison, speech unproved')


if __name__ == '__main__':
    main()
