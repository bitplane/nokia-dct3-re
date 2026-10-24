"""Qualify own NSB-3 wrong PIN2, cold retry persistence and recovery."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_pin2_change_check import check_pin2_card, check_trace, MENU_FRAME
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check


def check_chain(runs):
    if len(runs) != 4:
        raise ValueError('requires seed/wrong/cold-change/cold-restore')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        check_pin2_card((run / 'nvram/nsb3r4t/sim_card').read_bytes(),
                        changed=index == 2, attempts=2 if index == 1 else 3)
        if index:
            check_link(runs[index - 1], run)
            check_trace((run / 'error.log').read_text(), restore=index == 3, wrong=index == 1)
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
            frame_check(run / 'snap/nsb3_security_29.png',
                        'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e'
                        if index == 1 else
                        'd145e1eb7e1008b3271c709e01b483fdb017b2eb9e29e80b3f545da93ccff14f')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-recovery', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires rejected source and cold destination')
            prepare_reload(*args.runs, lambda card: check_pin2_card(card, attempts=2))
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PIN2 rejection: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own physical PIN2 rejection/cold recovery')


if __name__ == '__main__':
    main()
