"""Authenticated architecture restore and physical recovery after SIP cleanup."""
import argparse
import hashlib
from pathlib import Path
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki5210_pin_registration_check import (
    FRAMES, verify_coherent_late, verify_pin_idle_state, require_in_order,
)


def verify(run, *, incoming=False):
    text = (run / 'error.log').read_text(errors='replace')
    card = (run / 'nvram/noki5210_3/sim_card').read_bytes()
    verify_coherent_late(text, card)
    verify_pin_idle_state(text, call=True)
    digits = (() if incoming else tuple('5210_pin_physical: key=Keypad ' + digit
                                       for digit in '5551234'))
    require_in_order(text, digits + (
        '5210_pin_physical: key=Send',
        'gsm_call_adapter: ' + ('incoming ' if incoming else '') + 'state id=1 epoch=1 phase=connected',
        '5210_pin_state: event=saved', '5210_pin_state: event=restored',
    ))
    for phase in ('entered', 'recovered'):
        with Image.open(run / f'snap/5210_pin_{phase}.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != FRAMES[phase]:
                raise ValueError('unexpected authenticated SIP restoration recovery pixels')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--incoming', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.run_directory, incoming=args.incoming)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 PIN SIP restoration FAIL: {error}\n')
    print('5210 authenticated architecture restoration and physical recovery PASS; SIP dialog not restored')


if __name__ == '__main__':
    main()
