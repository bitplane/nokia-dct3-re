import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_refresh_trace_check as check


GOOD = "\n".join([
    "proactive REFRESH ready",
    "SIM status ins=c2 sw=910b",
    "header cla=a0 ins=12 p1=00 p2=00 p3=0b",
    "header cla=a0 ins=10 p1=00 p2=00 p3=05",
    "body ins=10 length=5",
    "SIM status ins=10 sw=9000",
    "terminal-response data=810309010002028281030100",
    "SIM status ins=14 sw=9000",
    "proactive DISPLAY TEXT ready",
])


class SimToolkitRefreshTraceCheckTest(unittest.TestCase):
    def test_complete_refresh_passes(self):
        check.verify(GOOD)

    def test_missing_reprofile_fails(self):
        with self.assertRaisesRegex(ValueError, "ins=10"):
            check.verify(GOOD.replace("header cla=a0 ins=10", "header cla=a0 ins=11"))


if __name__ == "__main__":
    unittest.main()
