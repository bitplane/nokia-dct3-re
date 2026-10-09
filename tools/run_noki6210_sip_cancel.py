"""Fresh NPE-3 research-HLE SIP cancellation/failure signaling; no speech claim."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6210_upload_contract import assess
from tools.noki6210_staged_check import verify
from tools.run_noki6210_acceptance import OPERATOR_SHA256, check_frame, check_registration


def check_product_result(run, restore_idle=False):
    from PIL import Image
    text = (run / 'error.log').read_text(errors='replace')
    verify(text, runtime=True, selftest=True)
    if restore_idle:
        from tools.noki6210_state_check import verify as verify_state
        verify_state(text, 'idle')
        restored = text.find('state_roundtrip: result=pass scenario=idle')
        paging = text.find('gsm_call_adapter: incoming state id=1 epoch=')
        if restored < 0 or paging <= restored:
            raise ValueError('incoming call did not start after idle restoration')
        if json.loads((run / 'sip-result.json').read_text()).get('epoch') != 2:
            raise ValueError('idle restoration did not invalidate the original host epoch')
    check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
    if '6210_sip_cancel: physical Exit' not in text:
        raise ValueError('missed-call notification was not physically dismissed')
    with Image.open(run / 'snap/6210_sip_missed_call.png') as frame:
        check_frame(frame, 'd96b7e931611a3ae3399fd08c4029a2390c6550345fd56040d80396afe89e056',
                    'one missed call')
    for name in ('6210_sip_registered_idle.png', '6210_sip_after_cancel.png'):
        with Image.open(run / 'snap' / name) as frame:
            check_frame(frame, OPERATOR_SHA256, name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--pjsua', type=Path, required=True)
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--restore-idle', action='store_true',
                        help='restore idle CPU/RAM/time before admitting the fresh SIP call')
    outgoing = parser.add_mutually_exclusive_group()
    outgoing.add_argument('--outgoing-busy', action='store_true',
                        help='physically dial into SIP 486 instead of receiving CANCEL')
    outgoing.add_argument('--outgoing-unavailable', action='store_true',
                          help='physically dial into SIP 480 instead of receiving CANCEL')
    outgoing.add_argument('--restore-outgoing-pending', action='store_true',
                          help='restore an unanswered outgoing SIP 180 call, clearing it without media')
    outgoing.add_argument('--restore-incoming-alerting', action='store_true',
                          help='restore a ringing incoming call without Answer or media')
    parser.add_argument('--http-port', type=int, default=18621)
    parser.add_argument('--sip-port', type=int, default=25621)
    args = parser.parse_args()
    outgoing_status = 486 if args.outgoing_busy else 480 if args.outgoing_unavailable else None
    if (outgoing_status or args.restore_outgoing_pending or args.restore_incoming_alerting) and args.restore_idle:
        parser.error('outgoing failures cannot use the incoming idle-restore fixture')
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        assess((root / 'roms/noki6210/6210_556c.fls').read_bytes(),
               (root / 'roms/noki6210/6210 virgin eeprom 005fa000.fls').read_bytes())
        run.mkdir(parents=True, exist_ok=False)
        (run / 'cfg').mkdir()
        shutil.copyfile(root / 'fixtures/noki6210_sip_cancel/npe3hle.cfg',
                        run / 'cfg/npe3hle.cfg')
        handset = [str((args.mame or root / 'mame/mame').resolve()), 'npe3hle',
                   '-rompath', str(root / 'roms'), '-nvram_directory', str(run / 'nvram'),
                   '-cfg_directory', str(run / 'cfg'), '-noreadconfig',
                   '-debug', '-debugger', 'none', '-autoboot_delay', '0',
                   '-autoboot_script', str(root / 'tools' / (
                       'noki6210_sip_incoming_alerting_restore.lua' if args.restore_incoming_alerting else
                       'noki6210_sip_outgoing_restore.lua' if args.restore_outgoing_pending else
                       'noki6210_outgoing_call_input.lua' if outgoing_status else
                       'noki6210_sip_idle_restore.lua' if args.restore_idle else
                       'noki6210_sip_cancel_observe.lua')),
                   '-state_directory', str(run / 'sta'),
                   '-snapshot_directory', str(run / 'snap'), '-seconds_to_run', '60',
                   '-video', 'none', '-sound', 'none', '-throttle', '-log', '-verbose',
                   '-http', '-http_port', str(args.http_port)]
        command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
                   '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
                   '--product', '6210',
                   *(['--restore-outgoing', '--sip-response', '180'] if args.restore_outgoing_pending else
                     ['--sip-response', str(outgoing_status)] if outgoing_status else
                     ['--incoming', '--restore-call', '--restore-phase', 'alerting', '--ready-file',
                      str(run / 'snap/6210_sip_registered_idle.png')] if args.restore_incoming_alerting else
                     ['--incoming', '--cancel-incoming', '--ready-file',
                      str(run / 'snap/6210_sip_registered_idle.png')]),
                   '--http-port', str(args.http_port), '--sip-port', str(args.sip_port),
                   '--', *handset]
        with (run / 'console.log').open('w') as output:
            subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT,
                           check=True, timeout=180)
        if outgoing_status or args.restore_outgoing_pending:
            from PIL import Image
            text = (run / 'error.log').read_text(errors='replace')
            verify(text, runtime=True, selftest=True)
            check_registration(text, (run / 'nvram/npe3hle/sim_card').read_bytes())
            if args.restore_outgoing_pending:
                if ('LUA ERROR' in text.upper() or '6210_state: FAIL' in text or
                        'state_roundtrip: result=pass scenario=call' not in text):
                    raise ValueError('pending call architecture did not restore exactly')
                before_save = text.split('6210_state: scenario=call event=saved', 1)[0]
                check_registration(before_save, (run / 'nvram/npe3hle/sim_card').read_bytes())
            if ('6210_call_physical: action=send' not in text or
                    '6210_keypad_decoded: key=0e' not in text):
                raise ValueError('outgoing SIP fixture did not physically decode Send')
            frame_name = ('6210_state_call_after_release.png' if args.restore_outgoing_pending
                          else '6210_after_outgoing_call.png')
            with Image.open(run / 'snap' / frame_name) as frame:
                check_frame(frame, OPERATOR_SHA256, 'registered idle after SIP failure')
        else:
            check_product_result(run, args.restore_idle)
            if args.restore_incoming_alerting:
                text = (run / 'error.log').read_text(errors='replace')
                if ('LUA ERROR' in text.upper() or '6210_state: FAIL' in text or
                        'state_roundtrip: result=pass scenario=incoming_alerting' not in text):
                    raise ValueError('incoming alerting architecture did not restore exactly')
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f'6210 SIP signaling FAIL: {error}; inspect {run}\n')
    print('6210 research-HLE SIP ' + ('incoming alerting restoration' if args.restore_incoming_alerting else
          'pending outgoing restoration' if args.restore_outgoing_pending else
          f'outgoing {outgoing_status}' if outgoing_status else 'CANCEL') +
          ' PASS; registered idle recovered, no speech acceptance')


if __name__ == '__main__':
    main()
