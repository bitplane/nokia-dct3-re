#!/usr/bin/env python3
"""Tests for the host-originated SMS trace checker."""

import unittest

try:
    from tools.radio_incoming_host_sms_trace_check import verify
except ModuleNotFoundError:
    from radio_incoming_host_sms_trace_check import verify


GOOD = """
gsm_call_adapter: network registered=1 arfcn=1
gsm_call_adapter: incoming sms id=1 result=accepted
gsm_call_adapter: incoming sms state id=1 epoch=1 phase=queued
GSM service downlink kind=16 sapi=3 pd=09 message=01
GSM service uplink sapi=3 pd=09 message=04 length=2
GSM service uplink sapi=3 pd=09 message=01 length=5
GSM service downlink kind=17 sapi=3 pd=09 message=04
gsm_call_adapter: incoming sms state id=1 epoch=1 phase=delivered
"""

RESTORED = """
gsm_call_adapter: network registered=1 arfcn=1
gsm_call_adapter: incoming sms id=1 result=accepted
gsm_call_adapter: incoming sms state id=1 epoch=1 phase=queued
state_roundtrip: result=pass
gsm_call_adapter: incoming sms state id=1 epoch=2 phase=queued
GSM service downlink kind=16 sapi=3 pd=09 message=01
GSM service uplink sapi=3 pd=09 message=04 length=2
GSM service uplink sapi=3 pd=09 message=01 length=5
GSM service downlink kind=17 sapi=3 pd=09 message=04
gsm_call_adapter: incoming sms state id=1 epoch=2 phase=delivered
"""


class IncomingHostSmsTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_ordered_lifecycle(self) -> None:
        verify(GOOD)

    def test_rejects_missing_firmware_uplink(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD.replace("GSM service uplink", "missing uplink"))

    def test_rejects_reordered_lifecycle(self) -> None:
        lines = GOOD.strip().splitlines()
        lines[4], lines[5] = lines[5], lines[4]
        with self.assertRaises(ValueError):
            verify("\n".join(lines))

    def test_rejects_missing_network_final_cp_ack(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD.replace("GSM service downlink kind=17", "missing"))

    def test_accepts_republished_restore_lifecycle(self) -> None:
        verify(RESTORED, require_restore=True)

    def test_restore_requires_new_epoch(self) -> None:
        with self.assertRaises(ValueError):
            verify(GOOD, require_restore=True)

    def test_restore_rejects_delivery_before_republished_queue(self) -> None:
        lines = RESTORED.strip().splitlines()
        lines[4], lines[8] = lines[8], lines[4]
        with self.assertRaises(ValueError):
            verify("\n".join(lines), require_restore=True)


if __name__ == "__main__":
    unittest.main()
