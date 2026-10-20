"""Own NSK-1 selected FDN erase/cancel and cold empty Search acceptance."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsk1_pin2_change_check import check_cold_link
from tools.nsk1_security_compare import check_phase
from tools.nsk1_phonebook_roundtrip import frame_check, EMPTY_BOOK
from tools.nsk1_fdn_record_check import check_trace, erased_card, EDITED_NUMBER, SAVED_LIST

CONFIRM = '681a7cd63a5fa836670be4b04fe262a02860b2572cb30330bff5fb5d62dc62d9'
ERASED_MENU = '41f808f910959a5ea31c3f7914b86d06ba2b8f434ec21ff832154425ff21577c'


def check_runs(seed, erase, cancel, cold):
    _, trace = check_phase(seed, (seed / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
    check_trace(trace, 'cold')
    frame_check(seed / 'snap/nsk1_fdn_final.png', EDITED_NUMBER)
    before = (seed / 'nvram/nsk1r4t/sim_card').read_bytes()
    erased = erased_card(before)
    for run, previous, mode in ((erase, seed, 'erase'), (cancel, seed, 'cancel'), (cold, erase, 'empty')):
        check_cold_link(run, previous)
        _, trace = check_phase(run, (run / 'initial_eeprom.bin').read_bytes(), through=59, minimum_records=1)
        check_trace(trace, mode)
        if (run / 'nvram/nsk1r4t/sim_card').read_bytes() != (before if mode == 'cancel' else erased):
            raise ValueError('selected erase/cancel/cold whole-card result differs')
        if mode != 'empty':
            frame_check(run / 'snap/nsk1_fdn_22.png', CONFIRM)
        else:
            frame_check(run / 'snap/nsk1_fdn_14.png', EMPTY_BOOK)
        frame_check(run / 'snap/nsk1_fdn_final.png', SAVED_LIST if mode == 'cancel' else ERASED_MENU)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('seed', 'erase', 'cancel', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        check_runs(args.seed, args.erase, args.cancel, args.cold)
    except (ValueError, OSError, KeyError) as error:
        print(f'NSK-1 selected FDN erase failed: {error}', file=sys.stderr)
        return 1
    print('NSK-1 selected FDN erase, cancellation and cold empty Search PASS')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
