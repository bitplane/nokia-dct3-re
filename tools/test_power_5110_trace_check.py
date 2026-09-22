import unittest

from tools.power_5110_trace_check import verify


PRESS = "input-press: t=8.000000 name=power port=1f\n"
RELEASE = "input-release: t=8.220000 name=power port=1e\n"
OFF = "ccont_power: event=off t=9.362577000\n"


class Power5110TraceCheckTest(unittest.TestCase):
    def test_short_press_keeps_rail(self):
        verify(PRESS + RELEASE, "short")

    def test_sustained_press_removes_rail_before_release(self):
        verify(PRESS + OFF + RELEASE, "long")

    def test_rejects_short_press_rail_drop(self):
        with self.assertRaisesRegex(ValueError, "short"):
            verify(PRESS + RELEASE + OFF, "short")

    def test_rejects_long_press_without_rail_drop(self):
        with self.assertRaisesRegex(ValueError, "sustained"):
            verify(PRESS + RELEASE, "long")

    def test_rejects_rail_drop_before_press(self):
        with self.assertRaisesRegex(ValueError, "before"):
            verify(OFF + PRESS + RELEASE, "long")


if __name__ == "__main__":
    unittest.main()
