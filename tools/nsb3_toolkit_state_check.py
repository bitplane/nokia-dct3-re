"""Native save/load of a fetched NSB-3 DISPLAY TEXT awaiting physical input."""
import argparse
import hashlib
from pathlib import Path
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_toolkit_check import check_storage
from tools.nsb3_pin_check import check_trace as check_pin
from tools.nse2_native_state_check import check_state
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.sim_toolkit_replay_check import check_replay

RETURNED_MENU = 'b2bde13413750ea3a6442a8a4d6c90362d4e8b16cde72bbf2f981a00089cecd0'


def check_replay_protocol(text):
    check_pin(text, True)
    check_replay(text,prefix='nsb3',terminal_profile=bytes.fromhex('0b41977700'),
                 reset_markers=('nsb3_board_reset_request:',))


def check(source, run):
    check_storage(source, run, 1)
    text = (run / 'error.log').read_text()
    check_replay_protocol(text)
    before = (run / 'initial_sim_card.bin').read_bytes()
    after = (run / 'nvram/nsb3r4t/sim_card').read_bytes()
    pixels = []
    for phase in ('reference', 'restored'):
        with Image.open(run / f'snap/nsb3_state_{phase}.png') as image:
            if image.size != (84, 48):
                raise ValueError('own frame geometry differs')
            pixels.append(image.convert('RGB').tobytes())
    report = check_state(text, before, after, *pixels, prefix='nsb3',
                         checkpoint='65.000000000', card_size=3570, menu_frame=RETURNED_MENU)
    frame_check(run / 'snap/nsb3_toolkit_state_prompt.png',
                '0b61ca941a012e9feffe2d13b834c720031b71295220fc32139decd034172b2c')
    state = run / 'sta/nsb3r4t/nsb3_native_ui.sta'
    if not state.is_file() or state.stat().st_size == 0:
        raise ValueError('native saved-state artifact missing')
    with state.open('rb') as stream:
        report['state_sha256'] = hashlib.file_digest(stream, 'sha256').hexdigest()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        report = check(args.source, args.run)
    except (OSError, ValueError, KeyError) as exc:
        parser.exit(1, f'NSB-3 Toolkit restoration: FAIL: {exc}\n')
    print(f"PASS: pending card/UI restoration, two physical completions and {report['io_replay_events']} exact I/O writes; no registration claim")


if __name__ == '__main__':
    main()
