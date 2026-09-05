import unittest

from tools.radio_outgoing_sms_timeout_trace_check import verify


GOOD = """
dsp_hle: GSM service establish sapi=0 pd=05 message=24 length=16 data=05247403331881080910101032547698
dsp_hle: TX packet type=1b payload=25 data=00800d3f012b2b
dspif_transport: TX pending type=1b payload=25 data=00800d0053290119000100069121436587090e110007815515 t=28.133
dsp_hle: TX packet type=1b payload=25 data=00800d0053290119000100069121436587090e110007815515 t=28.134
dsp_hle: GSM service uplink sapi=3 pd=09 message=01 length=28 data=290119000100069121436587090e11000781551532f40000a702c824
gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2 outcome=2 status_report=0 t=28.146
dspif_transport: TX pending type=1b payload=25 data=00800d0453290119000100069121436587090e110007815515 t=51.062
dspif_transport: TX pending type=1b payload=25 data=00800d1453290119000100069121436587090e110007815515 t=51.484
dspif_transport: RX enqueue type=80 payload=34 data=801200002b93000100000d71012b2b
"""


class OutgoingSmsTimeoutTraceCheckTest(unittest.TestCase):
    def test_bounded_retry(self):
        verify(GOOD)

    def test_rejects_repeat_storm(self):
        with self.assertRaisesRegex(ValueError, "controls"):
            verify(GOOD + "\n" + GOOD.splitlines()[8].replace("51.484", "52.3"))

    def test_rejects_early_cp_retry(self):
        with self.assertRaisesRegex(ValueError, "CP retry delay"):
            verify(GOOD.replace("t=51.062", "t=30.062"))


if __name__ == "__main__":
    unittest.main()
