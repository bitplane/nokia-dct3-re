#!/usr/bin/env python3
"""Tests for the host-decided USSD trace checker."""

import unittest

from tools.radio_host_ussd_trace_check import verify


GOOD = """
gsm_ss: request=ussd_host id=1 transaction=1b invoke=1 dcs=0f packed_length=5
gsm_call_adapter: ussd request id=1 epoch=1 dcs=0f packed_length=5
gsm_call_adapter: ussd response id=2 outcome=0 result=rejected
gsm_call_adapter: ussd response id=1 outcome=0 result=accepted
dsp_hle: LAPDm service Channel Release acknowledged nr=4
"""


class HostUssdTraceCheckTest(unittest.TestCase):
    def test_accepts_ordered_transaction(self) -> None:
        verify(GOOD)

    def test_rejects_missing_radio_release(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD.replace("Channel Release", "missing release"))

    def test_restore_requires_two_transport_epochs(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD, require_restore=True)
        verify(GOOD + GOOD.replace("epoch=1", "epoch=2"),
               require_restore=True)


if __name__ == "__main__":
    unittest.main()
