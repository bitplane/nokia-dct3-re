"""Own NSB-3 first wrong PUK2, cold recovery and authenticated restore."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_puk2_check import check_recovery
from tools.nsb3_pin2_change_check import check_pin2_card, check_trace as check_change, MENU_FRAME
from tools.nsb3_puk_check import check_link
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import ATTEMPTS_OFFSET
from tools.nsk1_phonebook_roundtrip import frame_check


def check_rejected_card(card, remaining=9):
    if len(card) <= ATTEMPTS_OFFSET + 3 or card[ATTEMPTS_OFFSET + 3] != remaining:
        raise ValueError('PUK2 retry state differs')
    canonical = bytearray(card)
    canonical[ATTEMPTS_OFFSET + 3] = 10
    check_pin2_card(canonical, attempts=0)


def check_chain(runs):
    if len(runs) != 4:
        raise ValueError('requires PIN2-blocked seed/wrong-PUK2/cold-recovery/cold-restore')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        if index < 2:
            check_rejected_card(card, 10 if index == 0 else 9)
        else:
            check_pin2_card(card, changed=index == 2)
        if not index:
            continue
        check_link(runs[index - 1], run)
        text = (run / 'error.log').read_text()
        if index < 3:
            check_recovery(text, wrong=index == 1)
        else:
            check_change(text, restore=True)
        frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
        if index == 1:
            frame_check(run / 'snap/nsb3_security_33.png',
                        'fd5b8fb1ab0c3397bf958d567a46aee84a72f75123802a00d5bc802a1814543e')
        elif index == 2:
            frame_check(run / 'snap/nsb3_security_33.png',
                        'dd6eba3e2fe5e7bdfbc0fc187d2dc63475c1227911a0deeaa4be70546075a0c4')
        elif index == 3:
            frame_check(run / 'snap/nsb3_security_29.png',
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
            prepare_reload(*args.runs, check_rejected_card)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PUK2 retry: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own wrong-PUK2 persistence/cold recovery')


if __name__ == '__main__':
    main()
