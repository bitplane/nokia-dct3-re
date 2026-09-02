import unittest

from tools.dsp_tone_trace_check import check


TRACE = """
dsp_tone: frequency=900/0 amplitude=0000 active=0/0
dsp_tone: frequency=900/0 amplitude=65ac active=1/0
dsp_tone: frequency=0/0 amplitude=65ac active=0/0
"""


class DspToneTraceCheckTest(unittest.TestCase):
    def test_organic_tone_contract(self):
        self.assertEqual(len(check(TRACE)), 3)

    def test_missing_start_fails(self):
        with self.assertRaisesRegex(ValueError, "tone start"):
            check(TRACE.replace("amplitude=65ac active=1/0", "amplitude=0000 active=0/0"))

    def test_missing_stop_fails(self):
        with self.assertRaisesRegex(ValueError, "tone stop"):
            check("\n".join(TRACE.splitlines()[:3]))


if __name__ == "__main__":
    unittest.main()
