#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_timer_trace_check as check


GOOD = "\n".join([
    "menu selection item=1 accepted",
    "proactive TIMER MANAGEMENT ready",
    "SIM status ins=c2 sw=9113",
    "terminal-response data=81030f270002028281030131",
    "SIM status ins=14 sw=9000",
])


class ToolkitTimerTraceCheckTest(unittest.TestCase):
    def test_accepts_capability_rejection(self):
        check.verify(GOOD)

    def test_rejects_success_claim(self):
        with self.assertRaises(ValueError):
            check.verify(GOOD.replace("030131", "030100"))


if __name__ == "__main__":
    unittest.main()
