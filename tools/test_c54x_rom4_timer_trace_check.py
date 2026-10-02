import unittest

from tools.c54x_rom4_timer_trace_check import check


def fixture(slot_period=5000 * 12 / 13_000_000, offset=0):
    period = 5000 * 12 / 13_000_000
    frames = [f"rom4_frame_timer: expiry={n} enabled=1 length=4999 pc=408d t={n * period:.6f}"
              for n in range(1, 65)]
    slots = [f"rom4_slot_timer: expiry={n} delay=4990 pc=408d t={(n + 24) * slot_period + offset:.6f}"
             for n in range(1, 17)]
    return "\n".join(frames + slots)


class TimerCadenceTests(unittest.TestCase):
    def test_observed_cadence(self):
        self.assertEqual(check(fixture())["slots"], 16)

    def test_old_one_tick_storm(self):
        with self.assertRaises(ValueError):
            check(fixture(slot_period=12 / 13_000_000))

    def test_missing(self):
        with self.assertRaises(ValueError):
            check("")

    def test_phase_error(self):
        with self.assertRaises(ValueError):
            check(fixture(offset=0.001))

    def test_sequence_gap(self):
        with self.assertRaises(ValueError):
            check(fixture().replace("slot_timer: expiry=8 ", "slot_timer: expiry=9 "))

    def test_reload_change(self):
        with self.assertRaises(ValueError):
            check(fixture().replace("length=4999", "length=5000"))


if __name__ == "__main__":
    unittest.main()
