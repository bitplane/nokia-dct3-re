"""Physical cold NSB-3 PUK1 exhaustion, retaining exact predecessor storage."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_puk_check import check_recovery, check_link
from tools.nsb3_puk_retry_check import check_rejected_card
from tools.nsb3_phonebook_check import prepare_reload, wire
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsk3_puk2_exhaust import trace_text, compress_trace

ROOT = Path(__file__).resolve().parents[1]
BLOCKED = '55b898bce51fda88f082ee5a0fc9e8b35a7a9a5cfa0f6799a2b93eba5373a3f8'
REJECTED = '19aea3a524806cfa5a9a69c445b4760be356da73c71a637b2b377db2dcfa1bde'


def stored(run):
    return (run / 'nvram/nsb3r4t/sim_card').read_bytes()


def check_own(run):
    if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
        raise ValueError('own executable changed')
    if 'nsb3_board_reset_request:' in trace_text(run):
        raise ValueError('software reset in qualification')


def execute(run, script, seconds):
    command = [sys.executable, str(ROOT / 'tools/run_mame_isolated.py'),
               '--mame-dir', str(ROOT / 'mame'), '--run-dir', str(run), '--',
               'nsb3r4t', '-rompath', str(ROOT / 'roms'), '-noreadconfig',
               '-video', 'none', '-sound', 'none', '-joystickprovider', 'none',
               '-midiprovider', 'none', '-nothrottle', '-debug', '-debugger', 'none',
               '-log', '-seconds_to_run', str(seconds), '-autoboot_delay', '0',
               '-autoboot_script', str(ROOT / 'tools' / script)]
    with (run / 'console.log').open('w') as output:
        subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT, check=True)


def check_rejection(run, previous, remaining):
    check_link(previous, run)
    check_own(run)
    check_rejected_card(stored(run), remaining)
    check_recovery(trace_text(run), wrong=True, remaining=remaining)
    frame_check(run / 'snap/nsb3_puk_stage_3.png', BLOCKED if remaining else
                'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e')


def check_permanent(run, previous, physical=False):
    check_link(previous, run)
    check_own(run)
    check_rejected_card(stored(run), 0)
    if stored(run) != stored(previous):
        raise ValueError('terminal cold card changed')
    text = trace_text(run)
    if any(row[0] in (0x20, 0x24, 0x26, 0x28, 0x2c, 4, 0x44, 0xd6, 0xdc, 0x32, 0xc2)
           for row in wire(text)):
        raise ValueError('terminal rejected card admits authentication or mutation')
    if physical:
        # No VERIFY is expected: the card is rejected before credentials.
        keys = tuple('Keypad ' + digit for digit in '4321') + ('Left Softkey / Menu',)
        contacts = re.findall(r'nsb3_pin_key: correct=0 key=(.*?) down=([01]) t=[0-9.]+ physical_only=1', text)
        if contacts != [(key, state) for key in keys for state in ('1', '0')]:
            raise ValueError('terminal physical PIN contact control missing')
        frame_check(run / 'snap/nsb3_pin_prompt.png', REJECTED)
        frame_check(run / 'snap/nsb3_pin_result.png',
                    'fa50e1eff164b7d5f4ce038606ecf72ec22e19549b6fda4965ed173c27718925')
    else:
        frame_check(run / 'snap/nsb3_sim_12.png', REJECTED)


def chain(seed, wrong, output, check_only=False, resume=False):
    check_link(seed, wrong)
    check_own(seed)
    check_own(wrong)
    check_rejected_card(stored(seed), 10)
    check_rejected_card(stored(wrong), 9)
    check_recovery(trace_text(wrong), wrong=True)
    if output.exists() and not (check_only or resume):
        raise ValueError('requires fresh output or explicit resume/check-only')
    previous = wrong
    for remaining in range(8, -1, -1):
        run = output / f'remaining-{remaining}'
        if not check_only and not (resume and run.exists()):
            prepare_reload(previous, run, lambda card: check_rejected_card(card, remaining + 1))
            execute(run, 'nsb3_puk_wrong_input.lua', 32)
        check_rejection(run, previous, remaining)
        if not check_only:
            compress_trace(run)
        print(f'PASS: physical wrong PUK1, remaining={remaining}, exact cold card', flush=True)
        previous = run
    for name, script, seconds, physical in (
            ('cold-rejected', 'nsb3_sim_observe.lua', 16, False),
            ('physical-rejected', 'nsb3_pin_input.lua', 22, True)):
        run = output / name
        if not check_only and not (resume and run.exists()):
            prepare_reload(previous, run, lambda card: check_rejected_card(card, 0))
            execute(run, script, seconds)
        check_permanent(run, previous, physical)
        if not check_only:
            compress_trace(run)
        previous = run
        print(f'PASS: {name}, unchanged terminal card and no credential admission', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'wrong', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    try:
        chain(args.seed.resolve(), args.wrong.resolve(), args.output.resolve(), args.check_only, args.resume)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        parser.exit(1, f'NSB-3 PUK1 exhaustion: FAIL: {exc}\n')


if __name__ == '__main__':
    main()
