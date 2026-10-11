"""Physical NSM-5 PIN verification followed by own laboratory registration."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.sim_security_trace_check import validate
from tools.sim_toolkit_trace_check import require_in_order
from tools.radio_registration_trace_check import verify as registration

FRAMES = {
    'entered': '00da16bf71bbf2ea5cc5efd9080724b626b84e555c5d49305e685d092dee0546',
    'idle': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
    'menu': '36956cb40f917975e07505c1889210989c2915d153f4578dc29350e175b70f47',
    'recovered': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
}


def verify(text, card):
    if '[LUA ERROR]' in text:
        raise ValueError('physical PIN fixture failed')
    validate(text, card, 'verify', '1234')
    require_in_order(text, (
        '5210_pin_physical: key=Keypad 1', '5210_pin_physical: key=Keypad 2',
        '5210_pin_physical: key=Keypad 3', '5210_pin_physical: key=Keypad 4',
        '5210_pin_physical: key=Left Softkey / Menu',
        'SIM status ins=20 sw=9000',
        'LAPDm Location Updating Accept acknowledged nr=1',
    ))
    registration(text, 'nsm5')


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'),
               (run / 'nvram/noki5210_3/sim_card').read_bytes())
        for phase, digest in FRAMES.items():
            with Image.open(run / f'snap/5210_pin_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                    raise ValueError('unexpected PIN/menu recovery pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 PIN registration FAIL: {error}\n')
    print('5210 physical SIM PIN and subsequent laboratory registration PASS; research HLE')


if __name__ == '__main__':
    main()
