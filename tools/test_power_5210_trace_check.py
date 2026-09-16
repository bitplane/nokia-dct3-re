import unittest

from tools.power_5210_trace_check import verify


SHORT = """
input-press: t=15.0 name=power port=1f
input-release: t=15.25 name=power port=1e
"""
LONG = SHORT + "ccont_power: event=off t=22.0\n"


class Power5210TraceCheckTest(unittest.TestCase):
    def test_accepts_short(self):
        verify(SHORT, "short")

    def test_accepts_long(self):
        verify(LONG, "long")

    def test_rejects_short_rail_drop(self):
        with self.assertRaisesRegex(ValueError, "short"):
            verify(LONG, "short")

    def test_rejects_long_without_rail_drop(self):
        with self.assertRaisesRegex(ValueError, "rail-off"):
            verify(SHORT, "long")


if __name__ == "__main__":
    unittest.main()
