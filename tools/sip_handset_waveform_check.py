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


def verify(root, product='3210', direction='outgoing', *, pcs1900=False):
    root = Path(root)
    if pcs1900:
        if product != '8890':
            raise ValueError('PCS waveform qualification requires own 8890 acceptance')
        acceptance = json.loads((root / 'acceptance.json').read_text())
        expected = {'machine': 'nsb6hle', 'scenario': direction + '-sip-media',
                    'laboratory_carrier': 600, 'native_dsp_complete': False,
                    'hle_media_tested': True, 'result': 'pass'}
        if any(acceptance.get(key) != value for key, value in expected.items()):
            raise ValueError('PCS waveform lacks matching own-band/direction media acceptance')
        if json.loads((root / 'sip-result.json').read_text()).get('passed') is not True:
            raise ValueError('PCS waveform lacks SIP media acceptance')
    result = {'scope': f'{product} HLE microphone/earpiece through real SIP; not native DSP speech',
              'direction': direction,
              'microphone_to_remote': inspect_tone(root / 'sip-microphone.wav', 440),
              'remote_to_earpiece': inspect_tone(root / 'sip-earpiece.wav', 660),
              'passed': True}
    if pcs1900:
        result['laboratory_carrier'] = 600
    (root / 'sip-waveform-result.json').write_text(json.dumps(result, indent=2) + '\n')
    print('OK - sustained microphone 440 Hz and earpiece 660 Hz crossed real SIP')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--product', choices=('3210', '3310', '3330', '3410', '5210', '8210', '8290', '8850', '8890'), default='3210')
    parser.add_argument('--direction', choices=('incoming', 'outgoing'), default='outgoing')
    parser.add_argument('--pcs1900', action='store_true')
    args = parser.parse_args()
    try:
        verify(args.run_dir, args.product, args.direction, pcs1900=args.pcs1900)
    except (OSError, ValueError, wave.Error) as error:
        parser.exit(1, f'FAIL - {error}\n')
