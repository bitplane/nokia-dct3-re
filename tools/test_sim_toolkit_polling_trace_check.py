import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_polling_trace_check as check


GOOD = "\n".join([
    "proactive POLL INTERVAL ready",
    "SIM status ins=c2 sw=910f",
    "terminal-response data=81030a03000202828103010004020105",
    "proactive POLLING OFF ready",
    "SIM status ins=14 sw=910b",
    "terminal-response data=81030b040002028281030100",
    "SIM status ins=14 sw=9000",
])


class SimToolkitPollingTraceCheckTest(unittest.TestCase):
    def test_complete_sequence_passes(self):
        check.verify(GOOD)

    def test_status_after_polling_off_fails(self):
        with self.assertRaisesRegex(ValueError, "continued"):
            check.verify(GOOD + "\nheader cla=a0 ins=f2 p1=00 p2=00 p3=16")


if __name__ == "__main__":
    unittest.main()
