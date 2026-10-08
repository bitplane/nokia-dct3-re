"""Own NSM-3 DISPLAY TEXT, successful dismissal and persisted registration."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.dct3_toolkit_check import verify_display_text
from tools.noki8210_registration_check import verify as verify_registration
from tools.noki8210_staged_check import verify as verify_stage

FRAMES = {
    'display': '0c609d0fc7f1f59534f7e29ccaa995d441ff52f701b76093661b2458380ff558',
    'after_dismiss': 'd6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300',
}


def verify(text, frames, storage):
    from PIL import Image
    verify_display_text(text, '8210')
    verify_stage(text, runtime=True, selftest=True, base_record=True)
    verify_registration(text, storage)
    for phase, digest in FRAMES.items():
        with Image.open(frames / f'8210_toolkit_{phase}.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('missing reviewed NSM-3 Toolkit frame: ' + phase)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        run = args.log.parent
        verify(args.log.read_text(errors='replace'), run / 'snap',
               (run / 'nvram/nsm3hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 Toolkit FAIL: {error}\n')
    print('8210 physical DISPLAY TEXT, terminal response and registered idle PASS')


if __name__ == '__main__':
    main()
