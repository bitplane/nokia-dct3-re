#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_browser_trace_check as check


class ToolkitBrowserTraceCheckTest(unittest.TestCase):
    def test_accepts_capability_rejection(self):
        check.verify("\n".join([
            "menu selection item=1 accepted",
            "proactive LAUNCH BROWSER ready",
            "SIM status ins=c2 sw=911d",
            "terminal-response data=81030e150002028281030131",
            "SIM status ins=14 sw=9000",
        ]))

    def test_rejects_success_claim(self):
        with self.assertRaises(ValueError):
            check.verify("\n".join([
                "menu selection item=1 accepted",
                "proactive LAUNCH BROWSER ready",
                "SIM status ins=c2 sw=911d",
                "terminal-response data=81030e150002028281030100",
                "SIM status ins=14 sw=9000",
            ]))


if __name__ == "__main__":
    unittest.main()
