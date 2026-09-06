import unittest

from tools.radio_no_reply_forward_trace_check import verify


GOOD = """
GSM service uplink sapi=0 pd=0b message=3b length=33 data=1b7b1c1aa11802010102010a301004012a830110840581551532f48501057f0100 t=1
gsm_ss: request=register transaction=1b invoke=1 service=2a number_length=5 active=1 t=2
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=39 t=3
gsm_call_adapter: incoming state id=1 epoch=2 phase=paging t=4
gsm_call_adapter: incoming state id=1 epoch=2 phase=alerting t=5
gsm_ss: incoming call forwarded condition=no-reply destination_length=5 t=10
GSM service downlink kind=15 sapi=0 pd=03 message=25 length=5 t=11
gsm_call_adapter: incoming state id=1 epoch=2 phase=forwarded reason=no-reply destination_length=5 t=12
"""


class RadioNoReplyForwardTraceCheckTest(unittest.TestCase):
    def test_accepts_complete_lifecycle(self):
        verify(GOOD)

    def test_requires_timer_parameter(self):
        with self.assertRaisesRegex(ValueError, "five-second timer"):
            verify(GOOD.replace("850105", "850106"))

    def test_rejects_forwarding_before_alerting(self):
        with self.assertRaisesRegex(ValueError, "event 4"):
            verify(GOOD.replace("phase=alerting", "phase=queued"))


if __name__ == "__main__":
    unittest.main()
