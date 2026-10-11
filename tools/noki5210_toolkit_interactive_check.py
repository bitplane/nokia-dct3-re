"""Own NSM-5 physical Toolkit text replies and reviewed registered-idle pixels."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_toolkit_check import verify_interactive
from tools.sim_toolkit_trace_check import require_in_order

FRAMES = {
    'display': '0c609d0fc7f1f59534f7e29ccaa995d441ff52f701b76093661b2458380ff558',
    'inkey': '564276bf766550d88a9984502474758ec27374c07a134c7f1b20042bd6d41459',
    'input': 'c26d453971ea775fd7bdee10235bba195eb70254ce522fda3fbc6cab13bc09f9',
    'entered': '65bc02fe52b99ee30451f5e9398c591a93a5796d74b7ecd9950a1404dc6f875c',
    'idle': '08737c08772c99df2b41fc8560587820c2c209f5f74c12893a657bd6f623e555',
}


def verify(text):
    compact = text.replace('[:sim_card] ', '')
    verify_interactive(compact, '5210')
    require_in_order(compact, (
        '5210_toolkit_interactive: action=inkey_5',
        '5210_toolkit_interactive: action=inkey_confirm',
        'terminal-response data=8103022200020282810301000d020435',
        '5210_toolkit_interactive: action=input_4',
    ))


def main():
    from PIL import Image
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory', type=Path)
    args = parser.parse_args()
    try:
        run = args.run_directory
        verify((run / 'error.log').read_text(errors='replace'))
        for phase, digest in FRAMES.items():
            with Image.open(run / f'snap/5210_toolkit_interactive_{phase}.png') as frame:
                if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != digest:
                    raise ValueError('unexpected own Toolkit pixels: ' + phase)
    except (OSError, ValueError) as error:
        parser.exit(1, f'5210 interactive Toolkit FAIL: {error}\n')
    print('5210 physical Toolkit text replies and registered-idle pixels PASS; research HLE, not native DSP')


if __name__ == '__main__':
    main()
