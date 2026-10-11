"""Verify physical NSM-3 Toolkit menu selection across emulator restoration."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki8210_toolkit_inkey_check import verify_protocol, MENU_FRAMES
from tools.noki8210_registration_check import verify as registration
from tools.noki8210_staged_check import verify as stage
from tools.dct3_toolkit_menu_state_check import verify_state


def verify(text):
    verify_protocol(text, menu=True)
    verify_state(text, '8210', 'toolkit_physical')


def main():
    from PIL import Image
    import hashlib
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    run = args.log.parent
    try:
        text = args.log.read_text(errors='replace')
        verify(text)
        stage(text, runtime=True, selftest=True, base_record=True)
        registration(text, (run / 'nvram/nsm3hle/sim_card').read_bytes())
        for phase in ('entry', 'items', 'reference', 'restored'):
            with Image.open(run / f'snap/8210_toolkit_menu_{phase}.png') as frame:
                expected = MENU_FRAMES['menu_entry' if phase == 'entry' else 'menu_items']
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
                    raise ValueError('unreviewed menu restoration pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 Toolkit restoration FAIL: {error}\n')
    print('8210 physical Toolkit selection, exact state/card replay and pixels PASS; native DSP unproved')


if __name__ == '__main__':
    main()
