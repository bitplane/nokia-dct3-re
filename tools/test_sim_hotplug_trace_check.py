import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_hotplug_trace_check as check


GOOD = "\n".join([
    "sim_presence_fixture: present=0",
    "SIM lifecycle event=deactivate",
    "SIM lifecycle event=inactive",
    "sim_detect: present=0 control=ab",
    "mad2_timer: event=ack mask=080",
    "sim_presence_fixture: present=1",
    "sim_detect: present=1 control=e3",
    "mad2_interrupt: event=ack mask=080",
    "SIM lifecycle event=activate",
    "read-binary fid=6fae offset=0 length=1 first=02",
])


class SimHotplugTraceCheckTest(unittest.TestCase):
    def test_complete_lifecycle_passes(self):
        check.verify(GOOD, 2)

    def test_presence_polarity_is_checked(self):
        with self.assertRaisesRegex(ValueError, "absent-card status"):
            check.verify(GOOD.replace("control=ab", "control=a3"), 2)

    def test_reinitialization_is_required(self):
        with self.assertRaisesRegex(ValueError, "incomplete"):
            check.verify(GOOD.replace("event=activate", "event=idle"), 2)

    def test_state_is_independently_required(self):
        with self.assertRaisesRegex(ValueError, "save-state"):
            check.verify(GOOD, 2, require_state=True)

    def test_phase3_requires_a_new_terminal_profile(self):
        phase3 = GOOD.replace("first=02", "first=03")
        with self.assertRaisesRegex(ValueError, "TERMINAL PROFILE"):
            check.verify(phase3, 3)
        check.verify(phase3 + "\nheader cla=a0 ins=10 p1=00 p2=00 p3=05\n"
                     "SIM status ins=10 sw=9000", 3)


if __name__ == "__main__":
    unittest.main()
