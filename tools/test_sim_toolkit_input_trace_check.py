import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_input_trace_check as check


EVENTS = "\n".join([
    "header cla=a0 ins=10 p1=00 p2=00 p3=05",
    "proactive DISPLAY TEXT ready",
    "header cla=a0 ins=12 p1=00 p2=00 p3=16",
    "terminal-response data=810301218002028281030100",
    "proactive GET INKEY ready",
    "SIM status ins=14 sw=9115",
    "header cla=a0 ins=12 p1=00 p2=00 p3=15",
    "header cla=a0 ins=14 p1=00 p2=00 p3=10",
    "terminal-response data=8103022200020282810301000d020435",
    "proactive GET INPUT ready",
    "SIM status ins=14 sw=911a",
    "header cla=a0 ins=12 p1=00 p2=00 p3=1a",
    "header cla=a0 ins=14 p1=00 p2=00 p3=11",
    "terminal-response data=8103032300020282810301000d03043432",
    "SIM status ins=14 sw=9000",
])


class SimToolkitInputTraceCheckTest(unittest.TestCase):
    def test_missing_get_input_response_fails(self):
        with self.assertRaisesRegex(ValueError, "3432"):
            check.verify(EVENTS.replace("3432", "3433"), [])

    def test_input_frame_is_required(self):
        with self.assertRaisesRegex(ValueError, "GET INPUT value"):
            check.verify(EVENTS, [])


if __name__ == "__main__":
    unittest.main()
