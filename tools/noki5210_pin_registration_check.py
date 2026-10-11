"""Physical NSM-5 PIN verification followed by own laboratory registration."""
import argparse
import hashlib
import re
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


def verify_authentication(text, card):
    if '[LUA ERROR]' in text:
        raise ValueError('physical PIN fixture failed')
    validate(text, card, 'verify', '1234')
    require_in_order(text, (
        '5210_pin_physical: key=Keypad 1', '5210_pin_physical: key=Keypad 2',
        '5210_pin_physical: key=Keypad 3', '5210_pin_physical: key=Keypad 4',
        '5210_pin_physical: key=Left Softkey / Menu',
        'SIM status ins=20 sw=9000',
    ))


def verify(text, card):
    verify_authentication(text, card)
    require_in_order(text, (
        'SIM status ins=20 sw=9000',
        'LAPDm Location Updating Accept acknowledged nr=1',
    ))
    registration(text, 'nsm5')


def verify_late_boundary(text, card):
    verify_authentication(text, card)
    require_in_order(text, (
        'TX packet type=57 payload=4 words=3 radio_phase=serving_bcch data=03050000',
        'RX enqueue type=8b payload=166',
        '5210_measurement_entry: pc=0037019c',
        '5210_measurement_entry: pc=002e336c',
        '5210_measurement_entry: pc=002bd782',
        '5210_request_entry: pc=002e1f5e',
        '5210_request_entry: pc=002bd3d8 r0=00000005',
        '5210_request_entry: pc=002bd424',
        'SIM status ins=20 sw=9000',
    ))
    stops = re.findall(r'5210_pin_fixture: stopped_at=([0-9.]+)', text)
    if len(stops) != 1 or float(stops[0]) < 44:
        raise ValueError('delayed-readiness observation ended before the bounded run')
    verify_late_request_ownership(text)
    if ('LAPDm Location Updating Accept acknowledged' in text or
            'radio_phase=random_access' in text):
        raise ValueError('late-readiness boundary changed: review radio progress rather than retaining this negative gate')


def verify_late_request_ownership(text):
    selectors = list(re.finditer(
        r'5210_readiness_selector: argument=([0-9a-f]{8}) lr=([0-9a-f]{8}) '
        r'state=(\d+) eligible=(\d+) current=([0-9a-f]{8}) queued=([0-9a-f]{8}) t=([0-9.]+)', text))
    finals = re.findall(
        r'5210_readiness_root: final current=([0-9a-f]{8}) queued=([0-9a-f]{8}) '
        r'previous=([0-9a-f]{8}) context=([0-9a-f]{8}) writes=(\d+) selections=(\d+)', text)
    if len(finals) != 1:
        raise ValueError('missing unique final request-root observation')
    current, queued, previous, context, writes, selections = finals[0]
    if int(writes) > 80 or int(selections) > 64 or len(selectors) != int(selections):
        raise ValueError('request ownership observation is truncated')
    if not int(current, 16) or not int(queued, 16) or current == queued or not int(context, 16):
        raise ValueError('late queued/current request boundary changed')
    pin = text.index('SIM status ins=20 sw=9000')
    after = [entry for entry in selectors if entry.start() > pin]
    if len(after) != 1:
        raise ValueError('late selector lifecycle changed')
    argument, caller, state, eligible, selected_current, selected_queued, _ = after[0].groups()
    if (argument != '00000003' or caller != '00232069' or state != '6' or
            not int(eligible) or selected_current != current or selected_queued != queued):
        raise ValueError('late selector no longer preserves the eligible queued request')
    if not any(entry.start() < pin and entry.group(1) == '00000000' for entry in selectors):
        raise ValueError('missing pre-PIN argument-zero control')


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    parser.add_argument('--late-boundary', action='store_true',
                        help='verify the unresolved delayed-readiness boundary, not registration')
    args = parser.parse_args()
    try:
        run = args.run_directory
        text = (run / 'error.log').read_text(errors='replace')
        card = (run / 'nvram/noki5210_3/sim_card').read_bytes()
        (verify_late_boundary if args.late_boundary else verify)(text, card)
        for phase, digest in ({} if args.late_boundary else FRAMES).items():
            with Image.open(run / f'snap/5210_pin_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                    raise ValueError('unexpected PIN/menu recovery pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 PIN registration FAIL: {error}\n')
    print('5210 delayed PIN/measurement boundary reproduced; registration unresolved' if args.late_boundary else
          '5210 physical SIM PIN and subsequent laboratory registration PASS; research HLE')


if __name__ == '__main__':
    main()
