"""NSB-7 own physical speech control and documented clocked HLE PCM."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki8890_pcm_check import TICK
from tools.nsb7_speech_control_check import verify_call


def verify(text):
    verify_call(text)
    enable = text.index('dsp_control_write: data=860b pc=002cb93e r4=00000008 ')
    disable = text.index('dsp_control_write: data=840a pc=002cb93e r4=00000008 ', enable)
    previous = (0, 0)
    for tick in TICK.finditer(text):
        up, down, pcm, data_clock, frame_clock, shape, clocks, idle = map(int, tick.groups())
        if not enable < tick.start() < disable:
            raise ValueError('PCM outside own speech lifecycle')
        if (data_clock, frame_clock, shape) != (512000, 8000, 64):
            raise ValueError('PCM differs from NSB-7 documented framing')
        if (pcm != up or up <= previous[0] or down < previous[1] or down > up or
                clocks != pcm * 160 * 64 or idle != pcm * 160 * 47):
            raise ValueError('inexact PCM serial/counter accounting')
        previous = (up, down)
    if previous[0] < 100 or previous[1] < 50:
        raise ValueError('insufficient sustained bidirectional PCM')
    return {'uplink': previous[0], 'downlink': previous[1],
            'pcm_clock': [512000, 8000], 'native_speech': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print(verify(args.log.read_text(errors='replace')))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PCM FAIL: {error}\n')


if __name__ == '__main__':
    main()
