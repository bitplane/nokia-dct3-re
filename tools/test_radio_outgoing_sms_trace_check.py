import unittest

from tools.radio_outgoing_sms_trace_check import verify


GOOD = """
dsp_hle: GSM service establish sapi=0 pd=05 message=24 length=16 data=05247403331881080910101032547698
dsp_hle: TX packet type=1b payload=25 data=00800d3f012b2b
dsp_hle: TX packet type=1b payload=25 data=00800d0053290119000100069121436587090e110007815515
dsp_hle: GSM service uplink sapi=3 pd=09 message=01 length=28 data=290119000100069121436587090e11000781551532f40000a702c824
gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2
dsp_hle: GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
dsp_hle: GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5
dsp_hle: GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904
dsp_hle: LAPDm service Channel Release acknowledged nr=2
"""


class OutgoingSmsTraceCheckTest(unittest.TestCase):
    def test_complete_transaction(self):
        verify(GOOD)

    def test_rejects_missing_submit_segment(self):
        without = "\n".join(
            line for line in GOOD.splitlines() if "005329" not in line)
        with self.assertRaisesRegex(ValueError, "first SMS-SUBMIT segment"):
            verify(without)

    def test_rejects_wrong_recipient(self):
        changed = GOOD.replace("destination=5551234", "destination=5551235")
        with self.assertRaisesRegex(ValueError, "decoded SMS-SUBMIT"):
            verify(changed)

    def test_rejects_duplicate_submit(self):
        with self.assertRaisesRegex(ValueError, "exactly one"):
            verify(GOOD + "gsm_sms_submit:\n")


if __name__ == "__main__":
    unittest.main()
