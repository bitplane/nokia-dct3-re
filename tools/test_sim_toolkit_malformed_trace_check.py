#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_malformed_trace_check as check


GOOD = "\n".join([
    "menu selection item=1 accepted",
    "proactive UNKNOWN REQUIRED TLV ready",
    "SIM status ins=c2 sw=9114",
    "terminal-response data=810310218002028281030132",
    "SIM status ins=14 sw=9000",
])


class ToolkitMalformedTraceCheckTest(unittest.TestCase):
    def test_accepts_data_rejection(self):
        check.verify(GOOD)

    def test_rejects_capability_result(self):
        with self.assertRaises(ValueError):
            check.verify(GOOD.replace("030132", "030131"))


if __name__ == "__main__":
    unittest.main()
