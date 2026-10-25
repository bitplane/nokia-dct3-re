import math
import tempfile
import unittest
import wave
from pathlib import Path

from tools.aac_reference_metrics import compare, read_native_s32, read_reference


class ReferenceMetricsTest(unittest.TestCase):
    def test_channels_are_not_averaged_together(self):
        report = compare([0, 4 / 32768] * 32, [0, 0] * 32, 2)
        self.assertTrue(report['channels'][0]['criterion_met'])
        self.assertFalse(report['channels'][1]['criterion_met'])

    def test_independent_limits(self):
        row = compare([0], [0], 1)['channels'][0]
        self.assertEqual(row['maximum_limit'], 2 / 32768)
        self.assertAlmostEqual(row['rms_limit'], 1 / (32768 * math.sqrt(12)))
        self.assertFalse(compare([2 / 32768], [0], 1)['channels'][0]['criterion_met'])

    def test_no_implicit_alignment_or_nonfinite_input(self):
        for native, reference, channels in [([], [], 1), ([0], [0, 0], 1),
                                             ([0], [0], 2), ([float('nan')], [0], 1),
                                             ([1.01], [0], 1)]:
            with self.assertRaises(ValueError):
                compare(native, reference, channels)

    def test_rms_boundary_is_strict(self):
        limit = 2**-15 / math.sqrt(12)
        below = math.nextafter(limit, 0)
        above = math.nextafter(limit, math.inf)
        self.assertTrue(compare([below], [0], 1)['channels'][0]['criterion_met'])
        for value in (limit, above):
            self.assertFalse(compare([value], [0], 1)['channels'][0]['criterion_met'])

    def test_peak_boundary_is_inclusive_for_both_signs(self):
        limit = 2**-14
        for sign in (-1, 1):
            samples = [sign * limit] + [0] * 4095
            self.assertTrue(compare(samples, [0] * 4096, 1)['channels'][0]['criterion_met'])
            samples[0] = sign * math.nextafter(limit, math.inf)
            self.assertFalse(compare(samples, [0] * 4096, 1)['channels'][0]['criterion_met'])

    def test_invalid_reference_samples_are_not_hidden(self):
        for value in (math.nan, math.inf, -math.inf, 1.01, -1.01):
            for native, reference in (([value], [0]), ([0], [value])):
                with self.assertRaises(ValueError):
                    compare(native, reference, 1)

    def test_neither_tail_can_be_silently_discarded(self):
        for native, reference in (([0, 0], [0]), ([0], [0, 0])):
            with self.assertRaisesRegex(ValueError, 'no automatic trimming'):
                compare(native, reference, 1)

    def test_original_precision_and_explicit_native_scale(self):
        with tempfile.TemporaryDirectory() as directory:
            reference = Path(directory) / 'reference.wav'
            with wave.open(str(reference), 'wb') as output:
                output.setparams((1, 3, 44100, 0, 'NONE', 'not compressed'))
                output.writeframes(b'\x01\x00\x00\xff\xff\xff\x00\x00\x80')
            self.assertEqual(read_reference(reference), ([2**-23, -2**-23, -1], 1, 44100))
            native = Path(directory) / 'native.bin'
            native.write_bytes((1 << 23).to_bytes(4, 'little', signed=True))
            self.assertEqual(read_native_s32(native, 23), [1])
            native.write_bytes(b'\x00')
            with self.assertRaises(ValueError):
                read_native_s32(native, 23)


if __name__ == '__main__':
    unittest.main()
