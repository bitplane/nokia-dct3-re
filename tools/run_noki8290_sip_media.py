"""Run NSB-7 own-source physical SIP media; not native DSP speech."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.nsb7_pin_services_check import check_source
from tools.nsb7_storage_check import inspect
from tools.prepare_physical_audio_config import prepare


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--nvram-source', type=Path, required=True,
                        help='own qualified nsb7hle NVRAM directory, not donor storage')
    parser.add_argument('--pjsua', type=Path, required=True)
    direction = parser.add_mutually_exclusive_group(required=True)
    direction.add_argument('--outgoing-media', action='store_true')
    direction.add_argument('--incoming-media', action='store_true')
    direction.add_argument('--restore-outgoing-media', action='store_true')
    direction.add_argument('--restore-incoming-media', action='store_true')
    direction.add_argument('--restore-outgoing-pending', action='store_true')
    direction.add_argument('--restore-incoming-alerting', action='store_true')
    parser.add_argument('--record-media', action='store_true')
    parser.add_argument('--sound', choices=('none', 'pulse'), default='none')
    parser.add_argument('--sip-port', type=int, default=25490)
    parser.add_argument('--http-port', type=int, default=18964)
    args = parser.parse_args()
    unanswered = args.restore_outgoing_pending or args.restore_incoming_alerting
    restoring = args.restore_outgoing_media or args.restore_incoming_media or unanswered
    if restoring and args.record_media:
        parser.error('restoration does not qualify uninterrupted waveform recording')
    root = Path(__file__).resolve().parents[1]
    run, source = args.run.resolve(), args.nvram_source.resolve()
    if run.exists():
        parser.error('run directory must be fresh')
    card = (source / 'nsb7hle/sim_card').read_bytes()
    check_source(card)
    _, _, report = inspect((source / 'nsb7hle/flash').read_bytes())
    if not report['checksum_valid'] or not report['prefix_valid']:
        parser.error('own-source journal checksums are invalid')
    run.mkdir(parents=True)
    shutil.copytree(source, run / 'nvram')
    (run / 'initial_sim_card.bin').write_bytes(card)
    manifest = {str(path.relative_to(source)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in sorted(source.rglob('*')) if path.is_file()}
    (run / 'initial-storage.json').write_text(json.dumps(manifest, indent=2) + '\n')
    prepare(root / 'fixtures/nsb7_host', run / 'cfg')
    incoming = args.incoming_media or args.restore_incoming_media or args.restore_incoming_alerting
    script = 'nsb7_pin_host_incoming_call.lua' if incoming else 'nsb7_pin_host_outgoing_call.lua'
    if args.restore_outgoing_media or args.restore_outgoing_pending:
        script = 'nsb7_sip_outgoing_restore.lua'
    elif args.restore_incoming_media:
        script = 'nsb7_sip_incoming_restore.lua'
    elif args.restore_incoming_alerting:
        script = 'nsb7_sip_incoming_alerting_restore.lua'
    command = [sys.executable, str(root / 'tools/run_sip_handset_gate.py'),
               '--pjsua', str(args.pjsua.resolve()), '--run-dir', str(run),
               '--sip-port', str(args.sip_port), '--http-port', str(args.http_port),
               '--product', '8290']
    if incoming:
        command += ['--incoming', '--ready-file', str(run / 'snap/nsb7_call_ready.png')]
    if args.record_media:
        command += ['--record-media']
    if args.restore_outgoing_media or args.restore_outgoing_pending:
        command += ['--restore-outgoing']
    elif args.restore_incoming_media or args.restore_incoming_alerting:
        command += ['--restore-call']
    if args.restore_outgoing_pending:
        command += ['--sip-response', '180']
    elif args.restore_incoming_alerting:
        command += ['--restore-phase', 'alerting']
    command += ['--', str(root / 'mame/mame'), 'nsb7hle', '-rompath', str(root / 'roms'),
                '-nvram_directory', 'nvram', '-cfg_directory', 'cfg', '-noreadconfig',
                '-video', 'none', '-sound', args.sound, '-throttle', '-seconds_to_run',
                '74' if incoming else '82', '-log', '-verbose', '-debug', '-debugger', 'none',
                '-http', '-http_port', str(args.http_port), '-autoboot_delay', '0',
                '-autoboot_script', str(root / 'tools' / script)]
    with (run / 'console.log').open('w') as output:
        subprocess.run(command, cwd=run, env=os.environ.copy(), stdout=output,
                       stderr=subprocess.STDOUT, check=True, timeout=180)
    subprocess.run([sys.executable, str(root / 'tools/nsb7_sip_check.py'),
                    str(root / 'roms/noki8290/8290f522a.fls'), str(run),
                    *(['--clocked-unanswered'] if unanswered else ['--media']),
                    *(['--incoming'] if args.incoming_media else []),
                    *(['--restore-incoming'] if args.restore_incoming_media else []),
                    *(['--restore-incoming-alerting'] if args.restore_incoming_alerting else []),
                    *(['--restore-outgoing-pending'] if args.restore_outgoing_pending else []),
                    *(['--restore-outgoing'] if args.restore_outgoing_media else [])], check=True, timeout=90)
    final_source = {str(path.relative_to(source)): hashlib.sha256(path.read_bytes()).hexdigest()
                    for path in sorted(source.rglob('*')) if path.is_file()}
    if final_source != manifest:
        raise RuntimeError('predecessor NVRAM changed during isolated acceptance')


if __name__ == '__main__':
    main()
