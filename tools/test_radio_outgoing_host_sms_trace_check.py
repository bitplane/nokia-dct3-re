import unittest

from tools.radio_outgoing_host_sms_trace_check import verify


GOOD = """
gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets=2
gsm_call_adapter: sms decision id=2 outcome=1 result=rejected
gsm_call_adapter: sms decision id=1 outcome=0 result=accepted
gsm_call_adapter: sms decision id=1 outcome=0 result=rejected
GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5
gsm_call_adapter: sms state id=1 epoch=1 phase=ended
"""


class HostSmsTraceCheckTest(unittest.TestCase):
    def test_complete(self):
        verify(GOOD)

    def test_missing_decision(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace("result=accepted", "result=rejected", 1))


if __name__ == "__main__":
    unittest.main()
