"""Bounded NSB-3 native state restoration, I/O replay and physical UI."""
import argparse
import hashlib
from pathlib import Path
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse2_native_state_check import check_state
from tools.nsb3_fdn_edit_check import check_card
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nsb3_puk_check import check_link
from tools.nsb3_nominal_input import IMAGE_SHA256

CALL_LOG = '7289ee054cf5c56164ac0bdc7f710693420e630678564812ff9192dc1a9f0e75'


def check(source, run):
    check_link(source, run)
    storage = run / 'nvram/nsb3r4t'
    if hashlib.sha256((storage / 'flash').read_bytes()).hexdigest() != IMAGE_SHA256:
        raise ValueError('own executable changed')
    before = (run / 'initial_sim_card.bin').read_bytes()
    after = (storage / 'sim_card').read_bytes()
    check_card(before)
    check_card(after)
    text = (run / 'error.log').read_text()
    check_pin(text, True)
    if 'nsb3_board_reset_request:' in text:
        raise ValueError('reset interrupted native restoration')
    pixels = []
    for phase in ('reference', 'restored'):
        with Image.open(run / f'snap/nsb3_state_{phase}.png') as image:
            if image.size != (84, 48):
                raise ValueError('own display geometry differs')
            pixels.append(image.convert('RGB').tobytes())
    return check_state(text, before, after, *pixels, prefix='nsb3',
                       checkpoint='40.000000000', card_size=3898, menu_frame=CALL_LOG)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        report = check(args.source, args.run)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 native restoration: FAIL: {exc}\n')
    print(f"PASS: native architecture, {report['io_replay_events']} I/O writes and physical Call log replay; no registration claim")


if __name__ == '__main__':
    main()
