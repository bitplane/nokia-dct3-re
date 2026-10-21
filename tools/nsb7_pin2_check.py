"""PIN2 change/rejection and independent cold use, preserving PIN1."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.nsb7_pin_change_check import check_change
from tools.nsb7_pin_check import serialized_profile, check_frame
from tools.nsb7_storage_check import inspect
from tools.sim_security_trace_check import CHV_OFFSET, ATTEMPTS_OFFSET, padded_pin


def check_cards(pairs):
    initial = serialized_profile()
    wrong = bytearray(initial)
    wrong[ATTEMPTS_OFFSET+1] = 2
    changed = bytearray(initial)
    changed[CHV_OFFSET+8:CHV_OFFSET+16] = padded_pin('8765')
    if pairs != [(initial, bytes(wrong)), (initial, bytes(changed)), (bytes(changed), initial)]:
        raise ValueError('complete PIN2 rejection/change/cold card transitions differ')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'wrong', 'changed', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check_change((args.wrong / 'error.log').read_text(), True, pin2=True)
        check_change((args.changed / 'error.log').read_text(), False, pin2=True)
        check_change((args.cold / 'error.log').read_text(), False, pin2=True, old='8765', new='5678')
        pairs = []
        for run in (args.wrong, args.changed, args.cold):
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            pairs.append(((run / 'initial_sim_card.bin').read_bytes(),
                          (run / 'nvram/nsb7hle/sim_card').read_bytes()))
        check_cards(pairs)
        check_frame(args.wrong / 'snap/nsb7_change_56.png',
                    '460778ecafd4fb9a23ec8c6632d3b4272b29c63c059b8c2eb764ad810f6c53c8')
        for run in (args.changed, args.cold):
            check_frame(run / 'snap/nsb7_change_56.png',
                        '693089e1bfb5848a4d945b894d21369ebe025961fb846d092570126b3cb152c0')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PIN2: FAIL: {error}\n')
    print('PASS: physical PIN2 change, rejected old PIN2 and cold new-PIN2 use; PIN1/files preserved')


if __name__ == '__main__':
    main()
