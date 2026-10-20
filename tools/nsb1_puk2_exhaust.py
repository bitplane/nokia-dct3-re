"""Exhaust saved NSB-1 PUK2 retries through isolated physical wrong entries."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb1_rom4_compare import prepare_cold_run, check_cold_link, check_card_run, check_frame, run_mame, INPUTS
from tools.nsb1_puk2_check import rejected_puk_card, exhausted_card
from tools.nsb1_pin_change_check import check_operation
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


def require_card(actual, expected):
    if actual != expected:
        raise ValueError('complete saved PUK2 card differs')


def initial_state(seed, exhausted, wrong):
    before = (seed / 'nvram/nsb1r4t/sim_card').read_bytes()
    require_card((exhausted / 'nvram/nsb1r4t/sim_card').read_bytes(), exhausted_card(before, 0))
    check_cold_link(wrong, exhausted)
    current = rejected_puk_card(before)
    require_card((wrong / 'nvram/nsb1r4t/sim_card').read_bytes(), current)
    code = json.loads((wrong / 'exit_status.json').read_text())['returncode']
    trace, card = check_card_run(wrong, code, 80)
    require_card(card, current)
    check_operation(trace, 0x2c, 0x9804, 24)
    return current


def check_chain(seed, exhausted, wrong, output):
    current = initial_state(seed, exhausted, wrong)
    previous = wrong
    for remaining in range(8, -1, -1):
        run = output / f'retries-{remaining}'
        check_cold_link(run, previous)
        code = json.loads((run / 'exit_status.json').read_text())['returncode']
        trace, card = check_card_run(run, code, 80)
        expected = bytearray(current)
        expected[ATTEMPTS_OFFSET + 3] = remaining
        require_card(card, expected)
        check_operation(trace, 0x2c, 0x9804 if remaining else 0x9840, 24)
        previous, current = run, bytes(expected)
    return previous


def check_permanent_cold(run, previous, back):
    check_cold_link(run, previous)
    code = json.loads((run / 'exit_status.json').read_text())['returncode']
    trace, card = check_card_run(run, code, 50)
    require_card(card, (previous / 'nvram/nsb1r4t/sim_card').read_bytes())
    if card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET + 4] != bytes((3, 0, 10, 0)):
        raise ValueError('permanently exhausted state not retained')
    check_operation(trace, None, 0x9000, 7 if back else 6)
    check_frame(run / 'snap/nsb1_pin_change_37.png',
                '49104c91455435c2fd67611ae734979d79e633446bc55c36afce6b375c2b628b')
    check_frame(run / 'snap/nsb1_pin_change_45.png', ACCESS_CODES_FRAME if back else
                '49104c91455435c2fd67611ae734979d79e633446bc55c36afce6b375c2b628b')


def exhaust(binary, seed, exhausted, wrong, output):
    if output.exists():
        raise ValueError('fresh exhaustion directory required')
    current = initial_state(seed, exhausted, wrong)
    output.mkdir(parents=True)
    previous = wrong
    results = []
    for remaining in range(8, -1, -1):
        run = output / f'retries-{remaining}'
        rompath = prepare_cold_run(run, previous, lambda card: require_card(card, current), INPUTS)
        code = run_mame(binary, rompath, run, Path(__file__).with_name('nsb1_puk2_wrong.lua').resolve(), 80, True)
        check_cold_link(run, previous)
        trace, card = check_card_run(run, code, 80)
        expected = bytearray(current)
        expected[ATTEMPTS_OFFSET + 3] = remaining
        require_card(card, expected)
        result = 0x9804 if remaining else 0x9840
        check_operation(trace, 0x2c, result, 24)
        current = bytes(expected)
        previous = run
        results.append({'run': str(run), 'puk2_retries': remaining, 'unblock_status': f'{result:04x}'})
        print(f'NSB-1 physical wrong PUK2: retries={remaining} sw={result:04x} whole_card=PASS', flush=True)
        (output / 'progress.json').write_text(json.dumps(results, indent=2) + '\n')
    return previous


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'seed', 'exhausted', 'wrong', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--check-only', action='store_true', help='requalify raw saved evidence without running MAME')
    parser.add_argument('--permanent-cold', type=Path, nargs=2, metavar=('SELECT', 'BACK'), help='qualify independent cold Select refusal and responsive Back controls')
    args = parser.parse_args()
    try:
        paths = [getattr(args, name).resolve() for name in ('binary', 'seed', 'exhausted', 'wrong', 'output')]
        if args.check_only:
            last = check_chain(*paths[1:])
        else:
            last = exhaust(*paths)
        if args.permanent_cold:
            for run, back in zip(args.permanent_cold, (False, True)):
                check_permanent_cold(run.resolve(), last, back)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f'NSB-1 organic PUK2 exhaustion failed: {error}', file=sys.stderr)
        return 1
    print(f'NSB-1 organic PUK2 exhaustion PASS; saved={last}')
    if args.permanent_cold:
        print('NSB-1 cold PIN2 item refuses Select while Back remains responsive; card unchanged PASS')
    else:
        print('Permanent cold behavior pending')
    return 0


ACCESS_CODES_FRAME = 'd966861348e41c4d3c12faa769072587a444387b4f31b23160de4f73a5d71c6d'

if __name__ == '__main__':
    raise SystemExit(main())
