"""Verify physical NSM-5 USSD after PIN authentication, not native speech."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki5210_pin_registration_check import FRAMES, verify_coherent_late
from tools.radio_ussd_trace_check import verify as verify_ussd
from tools.sim_toolkit_trace_check import require_in_order

USSD_FRAME = 'db9fb15970ea4c4a833c19c133030955305ade650c48f25c37ec0d2114773723'


def verify(run):
    from PIL import Image
    text = (run / 'error.log').read_text(errors='replace')
    verify_coherent_late(text, (run / 'nvram/noki5210_3/sim_card').read_bytes())
    require_in_order(text, (
        'SIM status ins=20 sw=9000',
        'LAPDm Location Updating Accept acknowledged nr=1',
        '5210_pin_physical: key=Keypad *',
        '5210_pin_physical: key=Keypad 1',
        '5210_pin_physical: key=Keypad 2',
        '5210_pin_physical: key=Keypad 3',
        '5210_pin_physical: key=Keypad #',
        '5210_pin_physical: key=Send',
        'gsm_ss: request=ussd',
        'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37',
        'radio_phase=release_channel_change',
        '5210_pin_physical: key=End',
    ))
    verify_ussd(text, run / 'snap', require_frame=False)
    for phase in ('entered', 'idle', 'recovered', 'ussd'):
        with Image.open(run / f'snap/5210_pin_{phase}.png') as image:
            if image.size != (84, 48):
                raise ValueError('wrong frame geometry: ' + phase)
            pixels = image.convert('L').tobytes()
        expected = USSD_FRAME if phase == 'ussd' else FRAMES[phase]
        frame = b'P5\n84 48\n255\n' + pixels if phase == 'ussd' else pixels
        if hashlib.sha256(frame).hexdigest() != expected:
            raise ValueError('unexpected authenticated USSD pixels: ' + phase)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        verify(args.run_directory)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 PIN USSD FAIL: {error}\n')
    print('5210 PIN USSD PASS: physical request, GSM exchange, result and idle recovery')


if __name__ == '__main__':
    main()
