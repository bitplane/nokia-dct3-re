"""Own NSM-2 open-menu restoration, physical selection and card replay."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import menu_events
from tools.dct3_toolkit_menu_state_check import verify_state
from tools.sim_toolkit_trace_check import require_in_order
from tools.radio_registration_trace_check import verify as registration


def verify(text):
    require_in_order(text.replace('[:sim_card] ', ''), menu_events('8850')[:-1])
    verify_state(text, '8850', 'toolkit_interactive')


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        text = (run / 'error.log').read_text(errors='replace')
        verify(text)
        registration(text, 'nsm2')
        storage = (run / 'nvram/nsm2hle/sim_card').read_bytes()
        if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
            raise ValueError('persisted EF_LOCI lacks updated laboratory location')
        for phase in ('entry', 'items', 'reference', 'restored'):
            expected = ('d79ec540f09fcf78dd5da15fc355030a99855022c105a6587f6d73a5574fa119'
                        if phase == 'entry' else '81615371eea5c88138ed06095c244d9235578d760f9a251716bf310f9daa00a9')
            with Image.open(run / f'snap/8850_toolkit_menu_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
                    raise ValueError('unreviewed menu restoration pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8850 Toolkit menu restoration FAIL: {error}\n')
    print('8850 physical menu selection, exact CPU/RAM and SIM replay PASS; native DSP unproved')


if __name__ == '__main__':
    main()
