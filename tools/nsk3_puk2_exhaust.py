"""Cold physical NSK-3 PUK2 exhaustion; never edit credential/retry state."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk3_boot_check import check_upload_handoffs
from tools.nsk3_cold_input import prepare
from tools.nsk3_pin2_check import check_change, PREFIX
from tools.nsk3_pin2_check import FRAME as PIN2_ITEM
from tools.nsk3_pin_check import check_trace as check_pin
from tools.nsk3_puk2_check import recovery
from tools.nsk3_security_check import check_trace as check_phone
from tools.nsk3_static_check import inspect
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, AOC_NVRAM_SIZE, statuses
from tools.nsk1_phonebook_roundtrip import frame_check

ROOT = Path(__file__).resolve().parents[1]
MACHINE = 'nsk3r4t'
BARRING_ITEM = '052dcbc2adb5dfd2acfc763260ab9dcb46a44ffc5ff1e57028e6146384008f95'


def trace_text(run):
    plain = run/'error.log'
    return plain.read_text() if plain.exists() else gzip.decompress((run/'error.log.gz').read_bytes()).decode()


def compress_trace(run):
    plain, archive = run/'error.log', run/'error.log.gz'
    if not plain.exists():
        trace_text(run)
        return
    source = plain.read_bytes()
    if not archive.exists():
        with archive.open('xb') as output:
            output.write(gzip.compress(source, mtime=0))
    if gzip.decompress(archive.read_bytes()) != source:
        raise ValueError('compressed trace does not reproduce its original bytes')
    plain.unlink()


def stored(run, name):
    return (run/'nvram'/MACHINE/name).read_bytes()


def check_link(run, previous, image):
    metadata = json.loads((run/'cold_input.json').read_text())
    hashes = {name: hashlib.sha256(stored(previous, name)).hexdigest()
              for name in ('flash', 'eeprom', 'sim_card')}
    if (metadata.get('source') != str(previous.resolve())
            or metadata.get('source_sha256') != hashes
            or metadata.get('copied_directories') != ['nvram', 'cfg']
            or metadata.get('ram_or_save_state_copied') is not False
            or metadata.get('fdn_fixture', False)):
        raise ValueError('cold source provenance or copied domains differ')
    if ((run/'initial_sim_card.bin').read_bytes() != stored(previous, 'sim_card')
            or (run/'initial_eeprom.bin').read_bytes() != stored(previous, 'eeprom')
            or stored(previous, 'flash') != image or stored(run, 'flash') != image):
        raise ValueError('cold card/EEPROM/own executable input differs')
    check_upload_handoffs(trace_text(run), 1)


def qualify_seed(source, pin_runs, wrong):
    if len(pin_runs) != 3:
        raise ValueError('requires all three physical PIN2 attempts')
    _, image = inspect(source, 'boot4')
    previous = None
    for count, run in zip((3, 2, 1), pin_runs):
        before = (run/'initial_sim_card.bin').read_bytes()
        if len(before) != AOC_NVRAM_SIZE or before[ATTEMPTS_OFFSET+1] != count:
            raise ValueError('PIN2 exhaustion skipped a retry')
        if previous is not None:
            check_link(run, previous, image)
        check_change(trace_text(run), before, stored(run, 'sim_card'),
                     '5675', '8765', False)
        check_upload_handoffs(trace_text(run), 1)
        if stored(run, 'flash') != image:
            raise ValueError('PIN2 seed executable differs')
        previous = run
    check_link(wrong, previous, image)
    check_pin(trace_text(wrong), True, allowed_mutations=(0x2c,))
    recovery(trace_text(wrong), stored(previous, 'sim_card'),
             stored(wrong, 'sim_card'), False)
    if stored(wrong, 'sim_card')[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] != bytes((3, 0, 10, 9)):
        raise ValueError('requires organically exhausted PIN2 and one physical wrong PUK2')
    return image


def execute(run, script):
    command = [sys.executable, str(ROOT/'tools/run_mame_isolated.py'),
        '--mame-dir', str(ROOT/'mame'), '--run-dir', str(run), '--', MACHINE,
        '-rompath', '../roms', '-video', 'none', '-sound', 'none', '-nothrottle',
        '-seconds_to_run', '52', '-log', '-verbose', '-debug', '-debugger', 'none',
        '-autoboot_delay', '0', '-autoboot_script', '../tools/'+script]
    with (run/'console.log').open('w') as console:
        subprocess.run(command, cwd=ROOT, stdout=console, stderr=subprocess.STDOUT, check=True)


def check_permanent(run, previous, image, navigate=False):
    check_link(run, previous, image)
    before = stored(previous, 'sim_card')
    if (before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] != bytes((3, 0, 10, 0))
            or stored(run, 'sim_card') != before):
        raise ValueError('cold permanent-block state or complete card changed')
    trace = trace_text(run)
    check_phone(trace, True, PREFIX)
    check_pin(trace, True)
    if (statuses(trace, 0x20) != [(0x9000, 3, 10)]
            or any(statuses(trace, ins) for ins in (4, 0x24, 0x26, 0x28, 0x2c, 0xdc, 0xd6))):
        raise ValueError('cold permanent refusal mutated credentials/storage or lost PIN1')
    contacts = re.findall(r'nsk3_puk2_permanent_contact: key=18 down=([01]) t=([0-9.]+) firmware_writes=0', trace)
    if (len(contacts) != trace.count('nsk3_puk2_permanent_contact:')
            or [edge for edge, _ in contacts] != (['1', '0'] if navigate else [])):
        raise ValueError('cold physical menu continuation differs')
    if navigate:
        times = [float(time) for _, time in contacts]
        if not 34 <= times[0] < 34.1 or not 34.2 <= times[1] < 34.3:
            raise ValueError('physical Down timing differs')
        window = trace.split('nsk3_puk2_permanent_contact:', 1)[1].split('nsk3_puk2_permanent_contact:', 1)[0]
        if not re.search(r'nsk3_logical_key: site=002ac2fc key=18 ', window):
            raise ValueError('physical Down was not decoded in the active menu')
    frame_check(run/'snap/nsk3_pin2_30.png', PIN2_ITEM)
    frame_check(run/'snap/nsk3_pin2_47.png', BARRING_ITEM if navigate else PIN2_ITEM)


def chain(source, pin_runs, wrong, output, *, check_only=False, resume=False, compress=False):
    image = qualify_seed(source, pin_runs, wrong)
    if output.exists() and not (check_only or resume):
        raise ValueError('use a fresh output; retained evidence requires --resume or --check-only')
    previous = wrong
    for remaining in range(8, -1, -1):
        run = output/f'remaining-{remaining}'
        if not check_only and not (resume and run.exists()):
            prepare(source, previous, run)
            execute(run, 'nsk3_puk2_wrong.lua')
        check_link(run, previous, image)
        check_pin(trace_text(run), True, allowed_mutations=(0x2c,))
        recovery(trace_text(run), stored(previous, 'sim_card'),
                 stored(run, 'sim_card'), False)
        if stored(run, 'sim_card')[ATTEMPTS_OFFSET+3] != remaining:
            raise ValueError('PUK2 exhaustion skipped a retry')
        print(f'PASS: NSK-3 physical wrong PUK2, remaining={remaining}, whole-card retry transition checked', flush=True)
        if compress:
            compress_trace(run)
        previous = run
    for navigate, name, script in ((False, 'permanent-cold', 'nsk3_pin2_prompt.lua'),
                                  (True, 'permanent-menu', 'nsk3_puk2_permanent_menu.lua')):
        permanent = output/name
        if not check_only and not (resume and permanent.exists()):
            prepare(source, previous, permanent)
            execute(permanent, script)
        check_permanent(permanent, previous, image, navigate)
        if compress:
            compress_trace(permanent)
        print(f'PASS: {name}, unchanged permanently blocked card and physical PIN1/menu', flush=True)
    return permanent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'first', 'second', 'blocked', 'wrong', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--compress-logs', action='store_true', help='verify and losslessly compress completed traces')
    args = parser.parse_args()
    try:
        chain(args.source.resolve(), [args.first.resolve(), args.second.resolve(), args.blocked.resolve()],
              args.wrong.resolve(), args.output.resolve(), check_only=args.check_only, resume=args.resume,
              compress=args.compress_logs)
    except (OSError, EOFError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f'NSK-3 PUK2 exhaustion: FAIL: {error}\n')


if __name__ == '__main__':
    main()
