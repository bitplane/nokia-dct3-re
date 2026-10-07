#!/usr/bin/env python3
"""Tests for the host-originated USSD trace checker."""

import unittest
from unittest.mock import patch

from tools.radio_host_incoming_ussd_trace_check import (
    SEVEN_TEXT_FRAME, verify, verify_seven_text_frame,
)


GOOD = """
gsm_call_adapter: network registered=1 arfcn=1
gsm_call_adapter: incoming ussd id=1 result=accepted
gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued
gsm_ss: network_initiated operation=notify
dsp_hle: LAPDm service Channel Release acknowledged nr=6
gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=delivered
"""

RESTORED = GOOD.replace(
    "gsm_ss: network_initiated",
    "state_roundtrip: result=pass\n"
    "gsm_call_adapter: incoming ussd state id=1 epoch=2 phase=queued\n"
    "gsm_ss: network_initiated",
).replace("epoch=1 phase=delivered", "epoch=2 phase=delivered")


class HostIncomingUssdTraceCheckTest(unittest.TestCase):
    def test_seven_text_frame_requires_exact_pixels_and_geometry(self):
        with patch('tools.radio_host_incoming_ussd_trace_check.sha256') as digest:
            digest.return_value.hexdigest.return_value = SEVEN_TEXT_FRAME
            verify_seven_text_frame(b'pixels', (84, 48))
            with self.assertRaises(ValueError):
                verify_seven_text_frame(b'pixels', (96, 60))
            digest.return_value.hexdigest.return_value = 'wrong'
            with self.assertRaises(ValueError):
                verify_seven_text_frame(b'pixels', (84, 48))

    def test_accepts_complete_transaction(self) -> None:
        verify(GOOD)

    def test_rejects_missing_firmware_notification(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD.replace("network_initiated", "missing"))

    def test_accepts_republished_restore_lifecycle(self) -> None:
        verify(RESTORED, require_restore=True)

    def test_restore_requires_new_epoch(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD, require_restore=True)


if __name__ == "__main__":
    unittest.main()
