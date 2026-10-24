"""Own NSB-6 call control and documented PCM transport; not native speech."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki8890_speech_control_check import verify_call

TICK = re.compile(
    r'dsp_hle: speech tick uplink=(\d+) downlink=(\d+) pcm=(\d+) '
    r'pcm_clock=(\d+)/(\d+) pcm_shape=(\d+) serial_clocks=(\d+)/(\d+)')


def verify(text, *, incoming=False):
    verify_call(text, incoming=incoming)
    enable = text.find('dsp_control_write: data=860b pc=002c338e r4=00000008 ')
    disable = text.find('dsp_control_write: data=840a pc=002c338e r4=00000008 ', enable)
    ticks = list(TICK.finditer(text))
    if not ticks:
        raise ValueError('missing executed NSB-6 PCM transfers')
    previous = (0, 0)
    for tick in ticks:
        up, down, pcm, data_clock, frame_clock, shape, clocks, idle = map(int, tick.groups())
        if not enable < tick.start() < disable:
            raise ValueError('PCM transfer outside own speech enable/disable lifecycle')
        if (data_clock, frame_clock, shape) != (512000, 8000, 64):
            raise ValueError('PCM clock/framing differs from own NSB-6 manual')
        if (pcm != up or up <= previous[0] or down < previous[1] or down > up or
                clocks != pcm * 160 * 64 or idle != pcm * 160 * 47):
            raise ValueError('PCM counters lack exact 64-clock/47-idle-clock accounting')
        previous = (up, down)
    if previous[0] < 100 or previous[1] < 50:
        raise ValueError('insufficient sustained bidirectional NSB-6 PCM transport')
    return {'uplink': previous[0], 'downlink': previous[1],
            'pcm_clock': [512000, 8000], 'clocks_per_frame': 64,
            'native_dsp_speech': False}


def verify_unanswered_restore(text):
    """Bound local 512k/64-clock PCM to post-load teardown, not external speech."""
    restored = text.find('sip_state: restored')
    released = text.find('LAPDm service Channel Release acknowledged', restored)
    if restored < 0 or released < 0:
        raise ValueError('missing unanswered restore/release boundaries')
    previous = 0
    for tick in TICK.finditer(text):
        up, down, pcm, data_clock, frame_clock, shape, clocks, idle = map(int, tick.groups())
        if not restored < tick.start() < released:
            raise ValueError('unanswered local PCM outside restored radio teardown')
        if (down != 0 or pcm != up or up <= previous or
                (data_clock, frame_clock, shape) != (512000, 8000, 64) or
                clocks != pcm * 160 * 64 or idle != pcm * 160 * 47):
            raise ValueError('unanswered PCM has downlink or invalid clock accounting')
        previous = up
    if re.search(r'media direction=\w+ [^\n]*result=accepted', text):
        raise ValueError('unanswered call admitted external media')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--incoming', action='store_true')
    args = parser.parse_args()
    try:
        print(verify(args.log.read_text(errors='replace'), incoming=args.incoming))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-6 PCM FAIL: {error}\n')


if __name__ == '__main__':
    main()
