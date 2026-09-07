#!/usr/bin/env python3
"""Tests for the host-originated USSD trace checker."""

import unittest

from tools.radio_host_incoming_ussd_trace_check import verify


GOOD = """
gsm_call_adapter: network registered=1 arfcn=1
gsm_call_adapter: incoming ussd id=1 result=accepted
gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued
gsm_ss: network_initiated operation=notify
dsp_hle: LAPDm service Channel Release acknowledged nr=6
gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=delivered
"""


class HostIncomingUssdTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_transaction(self) -> None:
        verify(GOOD)

    def test_rejects_missing_firmware_notification(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD.replace("network_initiated", "missing"))


if __name__ == "__main__":
    unittest.main()
