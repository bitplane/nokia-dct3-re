"""Own NSB-3 first wrong PUK1, cold recovery and replacement-PIN authentication."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_puk_check import check_recovery, check_link
from tools.nsb3_pin_check import check_card, check_trace as check_pin, PIN_PROMPT, PHONE_PROMPT, MENU_FRAME
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.sim_security_trace_check import ATTEMPTS_OFFSET
from tools.nsk1_phonebook_roundtrip import frame_check


def check_rejected_card(card, remaining=9):
    if len(card) <= ATTEMPTS_OFFSET + 2 or card[ATTEMPTS_OFFSET + 2] != remaining:
        raise ValueError('PUK1 attempts differ')
    canonical = bytearray(card)
    canonical[ATTEMPTS_OFFSET + 2] = 10
    check_card(bytes(canonical), 0)


def check_chain(runs):
    if len(runs) != 4:
        raise ValueError('requires blocked-PIN seed/wrong-PUK/cold-recovery/cold-PIN')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        if not index:
            check_card(card, 0)
            continue
        check_link(runs[index - 1], run)
        text = (run / 'error.log').read_text()
        if index == 1:
            check_rejected_card(card)
            check_recovery(text, wrong=True)
            frame_check(run / 'snap/nsb3_puk_stage_3.png',
                        '55b898bce51fda88f082ee5a0fc9e8b35a7a9a5cfa0f6799a2b93eba5373a3f8')
        elif index == 2:
            check_card(card, 3, '2468')
            check_recovery(text)
            frame_check(run / 'snap/nsb3_puk_stage_3.png', PHONE_PROMPT)
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
        else:
            check_card(card, 3, '2468')
            check_pin(text, True, pin='2468')
            frame_check(run / 'snap/nsb3_pin_prompt.png', PIN_PROMPT)
            frame_check(run / 'snap/nsb3_pin_result.png', PHONE_PROMPT)
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)


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
                raise ValueError('requires wrong-PUK source and cold destination')
            prepare_reload(*args.runs, check_rejected_card)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 PUK1 retry: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own wrong-PUK1/cold-recovery contract')


if __name__ == '__main__':
    main()
