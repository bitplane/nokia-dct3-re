import pathlib
import unittest

from tools.radio_smart_message_application_trace_check import verify


class SmartMessageApplicationTraceCheckTest(unittest.TestCase):
    def test_requires_two_deliveries(self):
        with self.assertRaisesRegex(ValueError, "two complete"):
            verify("", b"", "accepted")

    def test_rejects_unknown_outcome_after_common_frontier(self):
        log = (
            "GSM service downlink kind=16 sapi=3 pd=09 message=01 t=17.0\n"
            "GSM service downlink kind=16 sapi=3 pd=09 message=01 t=21.0\n"
            "LAPDm service Channel Release acknowledged nr=1\n"
            "buzzer: enabled=1 divider=8448 frequency=1538 volume=17 t=22.0\n")
        with self.assertRaisesRegex(ValueError, "unsupported"):
            verify(log, b"", "unknown")


if __name__ == "__main__":
    unittest.main()
