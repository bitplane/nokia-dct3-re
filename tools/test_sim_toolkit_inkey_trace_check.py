import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_inkey_trace_check as check


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
    "SIM status ins=14 sw=9000",
])


class SimToolkitInkeyTraceCheckTest(unittest.TestCase):
    def test_missing_second_fetch_fails(self):
        with self.assertRaisesRegex(ValueError, "p3=15"):
            check.verify(EVENTS.replace("p3=15", "p3=14"), [])

    def test_prompt_frame_is_required(self):
        with self.assertRaisesRegex(ValueError, "prompt"):
            check.verify(EVENTS, [])


if __name__ == "__main__":
    unittest.main()
