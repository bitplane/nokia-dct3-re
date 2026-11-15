import unittest

from tools.c54x_state_trace import first_divergence, parse

REFERENCE = """1048576 aaaa 0.010000000
2097152 bbbb 0.020000000
3145728 cccc 0.030000000
3200000 dddd end
"""


class StateTraceTest(unittest.TestCase):
    def test_identical_traces_agree(self):
        self.assertIsNone(first_divergence(REFERENCE, REFERENCE))

    def test_reports_the_first_divergent_window(self):
        candidate = REFERENCE.replace('bbbb', 'beef').replace('cccc', 'cafe')
        self.assertEqual(first_divergence(REFERENCE, candidate),
                         'first divergence between clocks 1048576 and 2097152 (emulated 0.010000000..0.020000000 s)')

    def test_reports_a_different_end(self):
        candidate = REFERENCE.replace('3200000 dddd', '3200001 dddd')
        self.assertIn('end differently', first_divergence(REFERENCE, candidate))

    def test_rejects_an_unfinished_trace(self):
        with self.assertRaises(ValueError):
            parse(REFERENCE.replace('3200000 dddd end\n', ''))


if __name__ == '__main__':
    unittest.main()
