"""Cold physical NSK-1 PUK2 exhaustion, with raw-evidence requalification."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin_roundtrip import check_run as check_pin, check_contacts
from tools.nsk1_pin_change_roundtrip import check_change_contacts, MENU
from tools.nsk1_pin2_change_check import check_cold_link
from tools.nsk1_puk2_check import check_exhaustion, check_recovery, REJECTED
from tools.nsk1_phonebook_roundtrip import copy_persistent, frame_check
from tools.nsk1_security_compare import run_machine, check_phase
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, statuses

PIN2_ITEM = '34ae4ee09f9be66dc8bb1322d5a17dfbb62ecfd2afd7f9d90884e7751819ada1'
SECURITY_ITEM = '030c9e9090a27b340cd6dead21ca39e856cdaea34a0cfe5ed5ad39d0a471b9dc'


def initial_state(source, two, one, zero, wrong):
    _, rejected_pin = check_pin(source / 'wrong')
    _, card = check_pin(source / 'correct', True, rejected_pin)
    if card[ATTEMPTS_OFFSET+1:ATTEMPTS_OFFSET+4] != bytes((3, 10, 10)):
        raise ValueError('healthy seed required')
    previous = source / 'correct'
    for run in (two, one, zero):
        check_exhaustion(run, previous)
        previous = run
    check_recovery(wrong, zero, wrong=True)
    return wrong


def chain(binary, source, two, one, zero, wrong, output, check_only=False, resume=False):
    previous = initial_state(source, two, one, zero, wrong)
    if not check_only:
        output.mkdir(parents=True, exist_ok=resume)
    for remaining in range(8, -1, -1):
        run = output / f'retries-{remaining}'
        if not check_only and not (resume and run.exists()):
            copy_persistent(run, previous)
            run_machine(binary, run, Path(__file__).with_name('nsk1_puk2_wrong.lua'), 78, True)
        check_recovery(run, previous, wrong=True)
        card = (run / 'nvram/nsk1r4t/sim_card').read_bytes()
        if card[ATTEMPTS_OFFSET+3] != remaining:
            raise ValueError('PUK2 chain skipped a retry')
        print(f'NSK-1 physical PUK2 exhaustion: remaining={remaining} whole_card=PASS', flush=True)
        previous = run
    return previous


def check_permanent_cold(run, previous, navigate):
    before = check_cold_link(run, previous)
    _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
    check_contacts(trace, 'pin', 5)
    check_contacts(trace, 'phone', 6)
    check_change_contacts(trace, pin2=True, entry_only=True, navigate=navigate)
    if (before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] != bytes((3, 0, 10, 0))
            or (run / 'nvram/nsk1r4t/sim_card').read_bytes() != before):
        raise ValueError('permanently exhausted card was not retained unchanged')
    if statuses(trace, 0x20) != [(0x9000, 3, 10)] or any(
            statuses(trace, ins) for ins in (4, 0x44, 0x24, 0x26, 0x28, 0x2c, 0xdc, 0xd6)):
        raise ValueError('cold refused menu issued a card mutation or failed startup authentication')
    frame_check(run / 'snap/nsk1_pin_change_17.png', REJECTED)
    frame_check(run / 'snap/nsk1_pin_change_20.png', SECURITY_ITEM if navigate else PIN2_ITEM)
    if navigate:
        menu = MENU[:]
        menu.insert(-1, 'down')
        expected = [1, 2, 3, 4, 0x19, 1, 2, 3, 4, 5, 0x19]
        expected += [0x19 if key == 'menu' else 0x18 for key in menu] + [0x18]
        rows = re.findall(r'nsk1_access_key: value=([0-9a-f]{2}) t=[0-9.]+ observer_writes=0', trace)
        if [int(key, 16) for key in rows] != expected or len(rows) != trace.count('nsk1_access_key:'):
            raise ValueError('physical security/menu/navigation decoding differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('binary', 'source', 'two', 'one', 'zero', 'wrong', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--check-only', action='store_true')
    parser.add_argument('--resume', action='store_true', help='requalify saved prefix before running missing cold attempts')
    parser.add_argument('--permanent-cold', nargs=2, type=Path, metavar=('SELECT', 'NAVIGATE'))
    args = parser.parse_args()
    try:
        last = chain(*(getattr(args, name).resolve() for name in
                       ('binary', 'source', 'two', 'one', 'zero', 'wrong', 'output')), check_only=args.check_only, resume=args.resume)
        if args.permanent_cold:
            for run, navigate in zip(args.permanent_cold, (False, True)):
                check_permanent_cold(run.resolve(), last, navigate)
    except (ValueError, OSError, KeyError, subprocess.SubprocessError) as error:
        print(f'NSK-1 PUK2 exhaustion failed: {error}', file=sys.stderr)
        return 1
    print(f'NSK-1 physical PUK2 exhaustion PASS; saved={last}')
    print('Cold PIN2 Select refusal and responsive navigation PASS' if args.permanent_cold else 'Cold refusal pending')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
