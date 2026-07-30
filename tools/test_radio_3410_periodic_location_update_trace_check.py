import unittest

from tools.radio_3410_periodic_location_update_trace_check import check
from tools.test_radio_periodic_location_update_trace_check import GOOD


CODE_BLOCK = (
    "dsp_hle: service code-block request=0001 event=initial t=1.4\n"
    + "\n".join(
        f"dspif_transport: IRQ4 service-complete request=0001 t={1.4 + i / 1000:.3f}"
        for i in range(151))
    + "\ndspif_transport: RAM W off=0e2 data=0000 t=1.6\n"
    + "dspif_transport: RAM W off=0e4 data=0004 t=1.6\n"
    + "ccont_watchdog: event=reload data=31 t=93.002\n"
)


class Nokia3410PeriodicLocationUpdateTraceCheckTest(unittest.TestCase):
    def test_accepts_finite_transfer_and_periodic_update(self):
        check(CODE_BLOCK + GOOD)

    def test_rejects_republished_selector(self):
        with self.assertRaisesRegex(ValueError, "one initial"):
            check(CODE_BLOCK + CODE_BLOCK.splitlines()[0] + "\n" + GOOD)

    def test_rejects_infinite_completion_stream(self):
        with self.assertRaisesRegex(ValueError, "151 completions"):
            check(CODE_BLOCK + "IRQ4 service-complete\n" + GOOD)

    def test_rejects_watchdog_expiry(self):
        with self.assertRaisesRegex(ValueError, "watchdog expired"):
            check(CODE_BLOCK + GOOD + "ccont_watchdog_expired: t=111\n")

    def test_rejects_missing_long_idle_reload(self):
        with self.assertRaisesRegex(ValueError, "sustain"):
            check(CODE_BLOCK.replace(
                "ccont_watchdog: event=reload data=31 t=93.002\n", "") + GOOD)


if __name__ == "__main__":
    unittest.main()
