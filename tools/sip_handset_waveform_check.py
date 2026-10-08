"""Require sustained received tones, not just executed media frame counts."""

import argparse
import array
import json
import math
from pathlib import Path
import sys
import wave


def inspect_tone(path, frequency, minimum_seconds=2):
    with wave.open(str(path), 'rb') as recording:
        if (recording.getnchannels(), recording.getsampwidth(), recording.getframerate()) != (1, 2, 8000):
            raise ValueError('expected mono 16-bit 8 kHz recording')
        samples = array.array('h', recording.readframes(recording.getnframes()))
    if sys.byteorder != 'little':
        samples.byteswap()
    longest = current = 0
    accepted = []
    for start in range(0, len(samples) - 7999, 8000):
        block = samples[start:start + 8000]
        energy = sum(value * value for value in block)
        rms = math.sqrt(energy / 8000)
        real = sum(value * math.cos(2 * math.pi * frequency * index / 8000)
                   for index, value in enumerate(block))
        imag = sum(value * math.sin(2 * math.pi * frequency * index / 8000)
                   for index, value in enumerate(block))
        fraction = 2 * (real * real + imag * imag) / (8000 * max(energy, 1))
        if rms >= 500 and fraction >= 0.5:
            current += 1
            longest = max(longest, current)
            accepted.append({'second': start // 8000, 'rms': rms, 'tone_energy_fraction': fraction})
        else:
            current = 0
    if longest < minimum_seconds:
        raise ValueError(f'missing sustained {frequency} Hz tone: longest={longest}s, required={minimum_seconds}s')
    return {'frequency': frequency, 'longest_seconds': longest, 'windows': accepted}


def verify(root, product='3210', direction='outgoing'):
    root = Path(root)
    result = {'scope': f'{product} HLE microphone/earpiece through real SIP; not native DSP speech',
              'direction': direction,
              'microphone_to_remote': inspect_tone(root / 'sip-microphone.wav', 440),
              'remote_to_earpiece': inspect_tone(root / 'sip-earpiece.wav', 660),
              'passed': True}
    (root / 'sip-waveform-result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('OK - sustained microphone 440 Hz and earpiece 660 Hz crossed real SIP')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--product', choices=('3210', '3310', '3330', '3410', '5210', '8210'), default='3210')
    parser.add_argument('--direction', choices=('incoming', 'outgoing'), default='outgoing')
    args = parser.parse_args()
    try:
        verify(args.run_dir, args.product, args.direction)
    except (OSError, ValueError, wave.Error) as error:
        parser.exit(1, f'FAIL - {error}\n')
