"""Own NSB-3 protected FDN erase cancellation, cold retention and confirmed erase."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_fdn_record_check import NUMBERS, READ_KEYS, check_record_trace
from tools.nsb3_fdn_edit_check import check_card as check_retained
from tools.nsb3_fdn_check import check_card as check_empty
from tools.nsb3_phonebook_check import prepare_reload
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsb3_menu_check import FRAME as MENU_FRAME

# Reviewed confirmation, retained Details, successful erase and cold-empty screens.
FRAMES = (
    ((24, '2501be7ff54218d5a541e0ae7c0d2a19430bfefe4b550b3d46942d172d88606c'),),
    ((15, 'cd1d7a1ec0c2c9a68010741763172a5ff97b387537ffab997ac088d16b116d27'),
     (16, '9b88b9c2b87def776295d6efba86b5728617a4f083fef151917a88721da7b7c2')),
    ((24, '2501be7ff54218d5a541e0ae7c0d2a19430bfefe4b550b3d46942d172d88606c'),
     (25, '922da30495f7290e0ab6f9b128f85f823ee66274495945c30365d8b6497e74d8')),
    ((15, 'ad7a1d617d2b9646c17bdcddec9e3a313dc2acc9932ac6e24c455beb33a8267d'),),
)

OPTIONS_KEYS = READ_KEYS + ('Left Softkey / Menu',) + tuple('Keypad ' + digit for digit in '5678') + ('Left Softkey / Menu',)
CANCEL_KEYS = OPTIONS_KEYS + ('Scroll Down', 'Left Softkey / Menu', 'Right Softkey / C')
ERASE_KEYS = OPTIONS_KEYS + ('Scroll Down', 'Left Softkey / Menu', 'Left Softkey / Menu')
EMPTY_KEYS = NUMBERS + ('Left Softkey / Menu',) * 2


def check_phase(text, phase):
    if phase == 0:
        check_record_trace(text, CANCEL_KEYS, authenticate_read=True)
    elif phase == 1:
        check_record_trace(text, READ_KEYS)
    elif phase == 2:
        check_record_trace(text, ERASE_KEYS, b'\xff' * 32)
    elif phase == 3:
        check_record_trace(text, EMPTY_KEYS)
    else:
        raise ValueError('unknown FDN erase phase')


def check_chain(runs):
    if len(runs) != 5:
        raise ValueError('requires edited source/cancel/cold-retained/erase/cold-empty')
    for index, run in enumerate(runs):
        if hashlib.sha256((run / 'nvram/nsb3r4t/flash').read_bytes()).hexdigest() != IMAGE_SHA256:
            raise ValueError('own executable changed')
        card = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
        if index < 3:
            check_retained(card)
        else:
            check_empty(card, True)
        if index:
            check_link(runs[index - 1], run)
            check_phase((run / 'error.log').read_text(), index - 1)
            frame_check(run / 'snap/nsb3_phonecode_menu.png', MENU_FRAME)
            for number, digest in FRAMES[index - 1]:
                frame_check(run / f'snap/nsb3_security_{number:02d}.png', digest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('prepare-retained', 'prepare-empty', 'check'))
    parser.add_argument('runs', nargs='+', type=Path)
    args = parser.parse_args()
    try:
        if args.operation == 'check':
            check_chain(args.runs)
        else:
            if len(args.runs) != 2:
                raise ValueError('requires source and cold destination')
            validator = check_retained if args.operation == 'prepare-retained' else lambda card: check_empty(card, True)
            prepare_reload(*args.runs, validator)
    except (OSError, ValueError, KeyError, StopIteration) as exc:
        parser.exit(1, f'NSB-3 FDN erase: FAIL: {exc}\n')
    print('PASS: ' + args.operation + ' own FDN cancel/retain/erase/cold-empty')


if __name__ == '__main__':
    main()
