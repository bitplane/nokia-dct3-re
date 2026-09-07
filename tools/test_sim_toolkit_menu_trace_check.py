import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_menu_trace_check as check


EVENTS = "\n".join([
    "terminal-response data=8103032300020282810301000d03043432",
    "proactive SET UP MENU ready",
    "SIM status ins=14 sw=9128",
    "header cla=a0 ins=12 p1=00 p2=00 p3=28",
    "terminal-response data=810304250002028281030100",
    "SIM status ins=14 sw=9000",
    "header cla=a0 ins=c2 p1=00 p2=00 p3=09",
    "envelope data=d30702020181100101",
    "menu selection item=1 accepted",
    "SIM status ins=c2 sw=9000",
])


class SimToolkitMenuTraceCheckTest(unittest.TestCase):
    def test_rejected_envelope_fails(self):
        with self.assertRaisesRegex(ValueError, "sw=9000"):
            check.verify(EVENTS.replace("SIM status ins=c2 sw=9000",
                                        "SIM status ins=c2 sw=6a80"), [])


if __name__ == "__main__":
    unittest.main()
