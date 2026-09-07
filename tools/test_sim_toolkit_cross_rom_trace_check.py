#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_cross_rom_trace_check as check


PREFIX = "\n".join([
    "read-binary fid=6fae offset=0 length=1 first=03",
    "header cla=a0 ins=10 p1=00 p2=00 p3=05",
    "SIM status ins=10 sw=9000",
    "proactive DISPLAY TEXT ready",
    "header cla=a0 ins=12 p1=00 p2=00 p3=16",
])


class ToolkitCrossRomTraceCheckTest(unittest.TestCase):
    def test_success(self):
        check.verify(PREFIX + "\nterminal-response data=810301218002028281030100\nSIM status ins=14 sw=9000", "success")

    def test_screen_busy(self):
        check.verify(PREFIX + "\nterminal-response data=81030121800202828103022001\nSIM status ins=14 sw=9000", "screen-busy")

    def test_outcomes_are_distinct(self):
        with self.assertRaises(ValueError):
            check.verify(PREFIX + "\nterminal-response data=81030121800202828103022001\nSIM status ins=14 sw=9000", "success")


if __name__ == "__main__":
    unittest.main()
