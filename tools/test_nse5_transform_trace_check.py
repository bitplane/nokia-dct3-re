import unittest

from tools.nse5_transform_trace_check import check_mix, check_trace, mix_words, rotate32


RECORD = (
    "nse5_compat_rotation: base=1202 input=218c:f9a8 output=6a08:633e "
    "other=1206 other_input=c22d:1de4 other_output=845a:3bc9 t=0.671721038\n"
)


class RotationTraceTests(unittest.TestCase):
    def test_observed(self):
        self.assertEqual(check_trace(RECORD), 1)

    def test_mismatch(self):
        with self.assertRaisesRegex(ValueError, "mismatch"):
            check_trace(RECORD.replace("633e", "633f"))

    def test_missing(self):
        with self.assertRaises(ValueError):
            check_trace("")

    def test_width(self):
        with self.assertRaises(ValueError):
            check_trace(RECORD.replace("218c:", ""))

    def test_wrap(self):
        self.assertEqual(rotate32(0x8000, 0, 31), (0, 1))

    def test_observed_mix(self):
        record = "nse5_compat_mix: input=e9b8:f1d4:27e2:e4fa:4cd7:54e6 output=218c:f9a8 t=0.671717596"
        self.assertEqual(check_mix(record), 1)
        with self.assertRaisesRegex(ValueError, "mismatch"):
            check_mix(record.replace("218c", "218d"))

    def test_mix_missing(self):
        with self.assertRaises(ValueError):
            check_mix("")

    def test_mix_width(self):
        with self.assertRaises(ValueError):
            mix_words((0, 0))

    def test_mix_zero(self):
        self.assertEqual(mix_words((0,) * 6), (0, 0))


if __name__ == "__main__":
    unittest.main()
