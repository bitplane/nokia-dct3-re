import unittest

from tools.radio_outgoing_sms_reject_trace_check import verify


GOOD = """
dsp_hle: GSM service establish sapi=0 pd=05 message=24 length=16 data=05247403331881080910101032547698
dsp_hle: TX packet type=1b payload=25 data=00800d3f012b2b
dsp_hle: TX packet type=1b payload=25 data=00800d0053290119000100069121436587090e110007815515
dsp_hle: GSM service uplink sapi=3 pd=09 message=01 length=28 data=290119000100069121436587090e11000781551532f40000a702c824
gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2 outcome=1
dsp_hle: GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
dsp_hle: GSM service downlink kind=19 sapi=3 pd=09 message=01 length=7
dsp_hle: GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904
dsp_hle: LAPDm service Channel Release acknowledged nr=2
"""


class OutgoingSmsRejectTraceCheckTest(unittest.TestCase):
    def test_complete_rejection(self):
        verify(GOOD)

    def test_rejects_success_ack(self):
        with self.assertRaisesRegex(ValueError, "success RP-ACK"):
            verify(GOOD + "GSM service downlink kind=18 sapi=3\n")

    def test_requires_error(self):
        with self.assertRaisesRegex(ValueError, "RP-ERROR"):
            verify(GOOD.replace("kind=19", "kind=20"))


if __name__ == "__main__":
    unittest.main()
