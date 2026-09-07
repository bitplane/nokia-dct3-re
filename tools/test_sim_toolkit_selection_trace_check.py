import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_event_list_trace_check as events
import sim_toolkit_select_item_trace_check as select


SELECT = "\n".join([
    "menu selection item=1 accepted",
    "proactive SELECT ITEM ready",
    "SIM status ins=c2 sw=9122",
    "header cla=a0 ins=12 p1=00 p2=00 p3=22",
    "terminal-response data=810307240002028281030100100102",
    "SIM status ins=14 sw=9000",
])
EVENTS = "\n".join([
    "menu selection item=1 accepted",
    "proactive SET UP EVENT LIST ready",
    "SIM status ins=c2 sw=910f",
    "header cla=a0 ins=12 p1=00 p2=00 p3=0f",
    "terminal-response data=810308050002028281030131",
    "SIM status ins=14 sw=9000",
])


class SimToolkitSelectionTraceCheckTest(unittest.TestCase):
    def test_select_item_passes(self):
        select.verify(SELECT)

    def test_wrong_selected_item_fails(self):
        with self.assertRaisesRegex(ValueError, "100102"):
            select.verify(SELECT.replace("100102", "100101"))

    def test_unsupported_event_list_passes_without_envelope(self):
        events.verify(EVENTS)

    def test_event_after_rejection_fails(self):
        with self.assertRaisesRegex(ValueError, "after rejecting"):
            events.verify(EVENTS + "\nenvelope data=d604990104")


if __name__ == "__main__":
    unittest.main()
