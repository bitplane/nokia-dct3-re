import unittest

from tools.ccont_watchdog_expiry_check import check


class CcontWatchdogExpiryCheckTest(unittest.TestCase):
    def test_accepts_retained_ready_status_and_restarted_clocks(self):
        log = """
ccont_power: event=cause_read data=13 t=1.0
ccont_watchdog_expired: t=3.0
mad2_clock: event=W off=0d data=0c old=00 counter=0000 pc=002b6434 t=3.1
ccont_power: event=cause_read data=13 t=3.2
"""
        self.assertEqual([], check(log)[0])

    def test_rejects_missing_restart_and_invented_cause(self):
        log = """
ccont_power: event=cause_read data=01 t=2.0
ccont_watchdog_expired: t=3.0
ccont_power: event=cause_read data=03 t=3.2
"""
        errors, _ = check(log)
        self.assertTrue(any("expected retained" in error for error in errors))
        self.assertTrue(any("clock initialization" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
