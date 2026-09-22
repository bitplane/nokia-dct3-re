import unittest

from tools.rom4_idle_input_trace_check import verify


CLOCK = "mad2_clock_ctrl: data=2c old=0c fiq=000 fmask=00 irq=000 imask=ca ctrl=0a t=8.539693\n"
PRESS = "input-press: t=12.000000 name=menu port=1f\n"
RELEASE = "input-release: t=12.220000 name=menu port=1b\n"


class Rom4IdleInputTraceCheckTest(unittest.TestCase):
    def test_accepts_late_awake_input(self):
        verify(CLOCK + PRESS + RELEASE)

    def test_rejects_early_input(self):
        with self.assertRaisesRegex(ValueError, "later idle"):
            verify(CLOCK + PRESS.replace("12.000000", "8.000000") + RELEASE)

    def test_rejects_clock_stop_request(self):
        with self.assertRaisesRegex(ValueError, "clock stop"):
            verify(CLOCK.replace("data=2c", "data=2e") + PRESS + RELEASE)

    def test_rejects_missing_clock_trace(self):
        with self.assertRaisesRegex(ValueError, "not traced"):
            verify(PRESS + RELEASE)


if __name__ == "__main__":
    unittest.main()
