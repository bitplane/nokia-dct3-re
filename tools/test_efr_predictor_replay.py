import struct
import unittest

from tools.run_efr_predictor_replay import compare


class PredictorComparisonTests(unittest.TestCase):
    def test_exact_captured_case_preserves_its_frame_identity(self):
        values = tuple(range(40))
        expected = [((97, 2), values)]
        result = compare(struct.pack('<40H', *values), expected)
        self.assertTrue(result['reference_equal'])
        self.assertFalse(result['full_decoder_conformance'])
        changed = (99,) + values[1:]
        result = compare(struct.pack('<40H', *changed), expected)
        self.assertEqual(result['differences'], [dict(frame=97, subframe=2, samples=1)])

    def test_missing_or_short_vectors_are_errors(self):
        for output, expected in ((b'', []), (b'', [((0, 0), (0,) * 39)]),
                                 (b'\x00' * 78, [((0, 0), (0,) * 40)])):
            with self.assertRaises(ValueError):
                compare(output, expected)
