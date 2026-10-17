import functools
import math
import unittest

from tools.c54x_rom4_pcm_clock_check import check, tone_fraction


KEYS = 'input-press: t=8.000000 name=2 port=1f\ninput-release: t=8.220000 name=2 port=0f\n'
CONTROL = ('rom4_tone_access: owner=mcu direction=write address=0100ac value=e10000 mask=ffff0000 pc=272034 t=8.100000\n'
           'rom4_tone_access: owner=mcu direction=write address=0100ac value=14e0 mask=ffff pc=272034 t=8.100100\n'
           'rom4_tone_access: owner=mcu_frequency direction=write address=0100b0 value=ae40000 mask=ffff0000 pc=272034 t=8.100200\n')


@functools.lru_cache(maxsize=2)
def clock_log(frequency=697):
    lines = [KEYS, CONTROL, 'rom4_tone_summary: tx_words=60000 rx_reads=60000 tone_reads=20 tone_copies=8 t=12.0']
    for n in range(52001):
        time = 2.5000625 + n / 8000
        value = round(1800 * (math.sin(2 * math.pi * frequency * time) + math.sin(2 * math.pi * 1336 * time))) if 8.11 <= time < 8.22 else 0
        lines.append(f'native_pcm_fixture_word: value={value & 0xffff:04x} t={time:.9f}')
    return '\n'.join(lines)


class NativeClockTests(unittest.TestCase):
    def test_native_waveform_and_unclocked_control(self):
        result = check(clock_log(), clock=True)
        self.assertEqual(result['active_samples'], 480)
        self.assertGreater(sum(result['sinusoidal_energy_fractions']), 0.99)
        self.assertFalse(result['handset_clock_claim'])
        self.assertFalse(result['native_call_speech_claim'])
        control = KEYS + CONTROL + 'rom4_tone_summary: tx_words=2 rx_reads=2 tone_reads=20 tone_copies=8 t=12.0'
        check(control, clock=False)
        with self.assertRaises(ValueError):
            check(control.replace('tx_words=2', 'tx_words=3'), clock=False)

    def test_wrong_frequency_and_non_sinusoidal_output(self):
        with self.assertRaisesRegex(ValueError, 'DTMF'):
            check(clock_log(450), clock=True)
        samples = [2000 if math.sin(2 * math.pi * 900 * n / 8000) >= 0 else -2000 for n in range(480)]
        self.assertLess(tone_fraction(samples), 0.9)
        self.assertEqual(tone_fraction([0] * 480), 0)

    def test_missing_or_retimed_samples_cannot_pass(self):
        for bad in (clock_log().replace('t=8.150062500', 't=8.150062600'),
                    clock_log().replace('native_pcm_fixture_word: value=0000 t=7.500062500', ''),
                    clock_log().replace('t=8.150062500', 't=8.150187500')):
            with self.assertRaises(ValueError):
                check(bad, clock=True)

    def test_key_and_silence_controls_required(self):
        for bad in (clock_log().replace('name=2', 'name=3'),
                    clock_log().replace('value=14e0', 'value=14e1'),
                    clock_log().replace('t=8.100100', 't=8.099900'),
                    clock_log().replace('tx_words=60000', 'tx_words=2'),
                    clock_log().replace('tone_copies=8', 'tone_copies=0'),
                    clock_log().replace('value=0000 t=7.500062500', 'value=0001 t=7.500062500'),
                    clock_log().replace('value=0000 t=8.600062500', 'value=0001 t=8.600062500')):
            with self.assertRaises(ValueError):
                check(bad, clock=True)


if __name__ == '__main__':
    unittest.main()
