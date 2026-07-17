import unittest

from tools.ccont_watchdog_trace_check import check


class CcontWatchdogTraceCheckTest(unittest.TestCase):
    def test_accepts_periodic_reload_without_expiry(self):
        log = "\n".join(
            f"ccont_watchdog_service: mask=03 caller=00237b2e task=02 t={8 + index * 3.965:.6f}"
            for index in range(12)
        )
        errors, _ = check(log, "soft_resets=0\n")
        self.assertEqual([], errors)

    def test_rejects_expiry_and_sparse_reload(self):
        errors, _ = check(
            "ccont_watchdog_service: mask=03 caller=0 task=02 t=8.0\n"
            "ccont_watchdog_expired: t=49.0\n",
            "soft_resets=1\n",
        )
        self.assertIn("CCONT watchdog expired", errors)
        self.assertIn("run did not retain zero soft resets", errors)

    def test_rejects_reset_count_with_zero_prefix(self):
        log = "\n".join(
            f"ccont_watchdog_service: mask=03 caller=0 task=02 t={index * 3.0:.6f}"
            for index in range(12)
        )
        errors, _ = check(log, "soft_resets=01\n")
        self.assertIn("run did not retain zero soft resets", errors)


if __name__ == "__main__":
    unittest.main()
