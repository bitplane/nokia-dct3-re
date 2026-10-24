"""NSB-7 own physical speech control and documented clocked HLE PCM."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.noki8890_pcm_check import TICK, verify_unanswered_restore
from tools.nsb7_speech_control_check import verify_call


def verify(text, *, incoming=False):
    verify_call(text, incoming=incoming)
    enable = text.index('dsp_control_write: data=860b pc=002cb93e r4=00000008 ')
    disable = text.index('dsp_control_write: data=840a pc=002cb93e r4=00000008 ', enable)
    return verify_ticks(text, enable, disable)


def verify_ticks(text, enable, disable):
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


def verify_before_restore(text, *, incoming=False):
    saved = text.find('sip_state: saved')
    if saved < 0:
        raise ValueError('missing actual save boundary')
    prefix = text[:saved]
    enable = prefix.find('dsp_control_write: data=860b pc=002cb93e r4=00000008 ')
    input_prefix = 'nsb7_call_input' if incoming else 'nsb7_outgoing_call_input'
    press = prefix.find(input_prefix + ': key=Send state=1 ')
    decoded = prefix.find('nsb7_key_decode: key=0e observer_writes=0', max(press, 0))
    if not 0 <= press < decoded < enable:
        raise ValueError('speech enable lacks ordered own physical Send/Answer')
    if enable < 0 or 'dsp_control_write: data=840a pc=002cb93e' in prefix[enable:]:
        raise ValueError('saved call lacks own enabled speech control')
    return verify_ticks(prefix, enable, len(prefix))


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
