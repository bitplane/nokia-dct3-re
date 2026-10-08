"""Own NSM-2 DISPLAY TEXT, physical dismissal and registered idle acceptance."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.dct3_toolkit_check import display_text_events, verify_display_text
from tools.radio_registration_trace_check import verify as verify_registration
from tools.noki8850_ussd_check import IDLE

EVENTS = display_text_events('8850')
DISPLAY = '0c609d0fc7f1f59534f7e29ccaa995d441ff52f701b76093661b2458380ff558'


def verify_protocol(text):
    verify_display_text(text, '8850')


def verify(text, frames, storage):
    from PIL import Image
    verify_protocol(text)
    verify_registration(text, 'nsm2')
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('persisted EF_LOCI lacks updated laboratory location')
    for phase, digest in (('display', DISPLAY), ('after_dismiss', IDLE)):
        with Image.open(frames / f'8850_toolkit_{phase}.png') as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                raise ValueError('missing reviewed NSM-2 Toolkit frame: ' + phase)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'), run / 'snap',
               (run / 'nvram/nsm2hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8850 Toolkit FAIL: {error}\n')
    print('8850 physical DISPLAY TEXT, terminal response and registered idle PASS')


if __name__ == '__main__':
    main()
