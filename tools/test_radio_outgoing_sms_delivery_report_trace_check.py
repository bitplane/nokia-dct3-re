import unittest

from tools.radio_outgoing_sms_delivery_report_trace_check import (
    EXPECTED_RECORD_PREFIX,
    SMS_OFFSET,
    verify,
)


GOOD = """
gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2 outcome=0 status_report=1
GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5
sim_device: update fid=6f3c record=1 length=176
LAPDm service Channel Release acknowledged nr=2
GSM service establish sapi=0 pd=06 message=27 length=16
gsm_sms_status_report: mr=00 recipient=5551234 status=00 length=37
GSM service downlink kind=16 sapi=3 pd=09 message=01 length=37
sim_device: update fid=6f3c record=1 length=176
GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904
GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020242
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
LAPDm service Channel Release acknowledged nr=3
"""


def card_with(prefix: bytes = EXPECTED_RECORD_PREFIX) -> bytes:
    record = prefix + bytes([0xff]) * (176 - len(prefix))
    return bytes(SMS_OFFSET) + record


class OutgoingSmsDeliveryReportTraceCheckTest(unittest.TestCase):
    def test_complete_report(self):
        verify(GOOD, card_with())

    def test_rejects_wrong_reference(self):
        with self.assertRaisesRegex(ValueError, "correlated status report"):
            verify(GOOD.replace("mr=00", "mr=01"), card_with())

    def test_rejects_missing_second_write(self):
        text = GOOD.replace(
            "sim_device: update fid=6f3c record=1 length=176\n", "", 1)
        with self.assertRaisesRegex(ValueError, "EF_SMS writes"):
            verify(text, card_with())

    def test_rejects_wrong_persisted_status(self):
        wrong = bytearray(EXPECTED_RECORD_PREFIX)
        wrong[-1] = 1
        with self.assertRaisesRegex(ValueError, "prefix mismatch"):
            verify(GOOD, card_with(bytes(wrong)))


if __name__ == "__main__":
    unittest.main()
