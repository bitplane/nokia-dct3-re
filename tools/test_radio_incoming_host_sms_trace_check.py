#!/usr/bin/env python3
"""Tests for the host-originated SMS trace checker."""

import unittest

try:
    from tools.radio_incoming_host_sms_trace_check import verify
except ModuleNotFoundError:
    from radio_incoming_host_sms_trace_check import verify


GOOD = """
gsm_call_adapter: incoming sms id=1 result=accepted
gsm_call_adapter: incoming sms state id=1 epoch=1 phase=queued
GSM service downlink kind=16 sapi=3 pd=09 message=01
GSM service uplink sapi=3 pd=09 message=01
gsm_call_adapter: incoming sms state id=1 epoch=1 phase=delivered
"""


class IncomingHostSmsTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_ordered_lifecycle(self) -> None:
        verify(GOOD)

    def test_rejects_missing_firmware_uplink(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD.replace("GSM service uplink", "missing uplink"))

    def test_rejects_reordered_lifecycle(self) -> None:
        lines = GOOD.strip().splitlines()
        lines[2], lines[3] = lines[3], lines[2]
        with self.assertRaises(ValueError):
            verify("\n".join(lines))


if __name__ == "__main__":
    unittest.main()
