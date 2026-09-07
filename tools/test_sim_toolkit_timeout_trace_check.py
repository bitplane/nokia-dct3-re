#!/usr/bin/env python3

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sim_toolkit_timeout_trace_check as check


def trace(fetch: float, response: float, result: str = "12") -> str:
    return (f"header cla=a0 ins=12 p1=00 p2=00 p3=16 t={fetch:.3f}\n"
            f"body ins=14 length=12 t={response:.3f}\n"
            "terminal-response data=81 03 01 21 80 02 02 82 81 03 01 " + result)


class ToolkitTimeoutTraceCheckTest(unittest.TestCase):
    def test_accepts_bounded_timeout(self):
        check.verify(trace(15.2, 73.58))

    def test_rejects_wrong_result(self):
        with self.assertRaises(ValueError):
            check.verify(trace(15.2, 73.58, "00"))

    def test_rejects_early_response(self):
        with self.assertRaises(ValueError):
            check.verify(trace(15.2, 30.0))


if __name__ == "__main__":
    unittest.main()
