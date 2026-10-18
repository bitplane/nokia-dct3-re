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

    def test_rejects_staged_dsp_faults(self):
        for fault in ('unsupported staged C54x form fe44', 'staged ADDM BRC is unsupported', 'unimplemented C54x operation'):
            with self.subTest(fault=fault), self.assertRaisesRegex(ValueError, 'illegal instruction'):
                verify(fault + '\n' + PRESS + RELEASE, 'short')

    def test_rejects_reset_even_when_power_actions_succeed(self):
        for kind, actions in (("short", PRESS + RELEASE),
                              ("long", PRESS + OFF + RELEASE)):
            with self.subTest(kind=kind), self.assertRaisesRegex(ValueError, "reset"):
                verify("rom4_reset_request: reason=00000004 t=0.349649\n" + actions, kind)


if __name__ == "__main__":
    unittest.main()
