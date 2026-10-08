"""Own NSM-3 physical call-forwarding interrogation acceptance."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools import radio_call_divert_trace_check as protocol
from tools.noki8210_supplementary_check import verify_transaction


def verify(text, frames):
    verify_transaction(text, frames, 'divert',
                       ('Keypad *', 'Keypad #', 'Keypad 2', 'Keypad 1', 'Keypad #', 'Call / Send'),
                       protocol,
                       '7213425cd2f688b40a09db063014725fee63d3babe121a1cb499d50e4b1dcb0b',
                       '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors='replace')
        verify(text, args.log.parent / 'snap')
        from tools.noki8210_registration_check import verify as verify_registration
        verify_registration(text, (args.log.parent / 'nvram/nsm3hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 divert FAIL: {error}\n')
    print('8210 physical call-divert interrogation protocol PASS')


if __name__ == '__main__':
    main()
