import unittest

from tools.ccont_watchdog_trace_check import check


class CcontWatchdogTraceCheckTest(unittest.TestCase):
    def test_accepts_combined_reload_without_expiry(self):
        log = "ccont_watchdog_service: mask=03 caller=00237b2e task=02 t=31.0\n"
        errors, _ = check(log, "soft_resets=0\n")
        self.assertEqual([], errors)

    def test_rejects_expiry_and_terminal_path(self):
        errors, _ = check(
            "ccont_watchdog_service: mask=03 caller=0 task=02 t=8.0\n"
            "watchdog_terminal: reason=68 caller=00237ba8 task=02 t=31.9\n"
            "ccont_watchdog_expired: t=49.0\n",
            "soft_resets=1\n",
        )
        self.assertIn("CCONT watchdog expired", errors)
        self.assertIn("firmware entered a terminal watchdog path", errors)
        self.assertIn("run did not retain zero soft resets", errors)

    def test_rejects_reset_count_with_zero_prefix(self):
        log = "ccont_watchdog_service: mask=03 caller=0 task=02 t=31.0\n"
        errors, _ = check(log, "soft_resets=01\n")
        self.assertIn("run did not retain zero soft resets", errors)


if __name__ == "__main__":
    unittest.main()
