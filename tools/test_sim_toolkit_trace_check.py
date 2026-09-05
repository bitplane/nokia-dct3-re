import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_trace_check as check


EVENTS = "\n".join([
    "sim_device: read-binary fid=6fae offset=0 length=1 first=03",
    "sim_device: header cla=a0 ins=10 p1=00 p2=00 p3=05",
    "SIM status ins=10 sw=9000",
    "sim_device: proactive DISPLAY TEXT ready",
    "SIM status ins=d6 sw=9116",
    "sim_device: header cla=a0 ins=12 p1=00 p2=00 p3=16",
    "sim_device: header cla=a0 ins=14 p1=00 p2=00 p3=0c",
    "sim_device: terminal-response data=810301218002028281030100",
    "SIM status ins=14 sw=9000",
])


class SimToolkitTraceCheckTest(unittest.TestCase):
    def test_missing_transaction_fails(self):
        with self.assertRaisesRegex(ValueError, "ins=12"):
            check.verify(EVENTS.replace("ins=12", "ins=13"), [])

    def test_state_is_independently_required(self):
        with self.assertRaisesRegex(ValueError, "save-state"):
            check.verify(EVENTS, [], require_state=True)

    def test_removal_cancels_pending_command(self):
        removal = "\n".join([
            "header cla=a0 ins=10 p1=00 p2=00 p3=05",
            "proactive DISPLAY TEXT ready",
            "sim_presence_fixture: present=0",
            "SIM lifecycle event=deactivate",
            "SIM lifecycle event=inactive",
        ])
        check.verify(removal, [], removal=True)
        with self.assertRaisesRegex(ValueError, "stale proactive"):
            check.verify(removal + "\nheader cla=a0 ins=12", [], removal=True)


if __name__ == "__main__":
    unittest.main()
