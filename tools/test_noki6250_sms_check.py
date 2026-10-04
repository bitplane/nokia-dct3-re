import unittest
from hashlib import sha256
from unittest.mock import patch

from tools.noki6250_sms_check import verify, verify_sent
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET

LOG = """sim_device: update fid=6f3c record=1 length=176
GSM service uplink sapi=3 pd=09 message=04 length=2 data=8904
GSM service uplink sapi=3 pd=09 message=01 length=5 data=8901020240
GSM service downlink kind=17 sapi=3 pd=09 message=04
LAPDm service Channel Release acknowledged nr=3
PCH IMSI page transmitted channel=60
sim_device: update fid=6f3c record=1 length=176
"""

SENT_LOG = """6250_sms_input: step=25 pressed=1
GSM service establish sapi=0 pd=05 message=24 length=16 data=052474
GSM service uplink sapi=3 pd=09 message=01 length=28 data=390119000100069121436587090e11010781551532f40000ff02c834
gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=2
GSM service downlink kind=17 sapi=3 pd=09 message=04
GSM service downlink kind=18 sapi=3 pd=09 message=01
GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904
LAPDm service Channel Release acknowledged nr=2
"""


class SmsCheckTests(unittest.TestCase):
    def test_sent_protocol_and_frame(self):
        pixels = bytes(96 * 60)
        with patch("tools.noki6250_sms_check.SENT_FRAME", sha256(pixels).hexdigest()):
            verify_sent(SENT_LOG, pixels, (96, 60))

    def test_wrong_sent_text_rejected(self):
        with self.assertRaisesRegex(ValueError, "Hi SMS-SUBMIT"):
            verify_sent(SENT_LOG.replace("ff02c834", "ff02c824"),
                        bytes(96 * 60), (96, 60))

    def test_missing_rp_ack_rejected(self):
        with self.assertRaisesRegex(ValueError, "RP-ACK"):
            verify_sent(SENT_LOG.replace("kind=18", "kind=99"),
                        bytes(96 * 60), (96, 60))

    def record(self, status):
        return bytes(SMS_NVRAM_OFFSET) + bytes([status]) + FIRST_SMS_DELIVER_BODY + bytes(176)

    def test_read_status_and_frame(self):
        pixels = bytes(96 * 60)
        with patch("tools.noki6250_sms_check.READ_FRAME", sha256(pixels).hexdigest()):
            verify(LOG, self.record(1), pixels, (96, 60))

    def test_deleted_status_and_frame(self):
        pixels = bytes(96 * 60)
        with patch("tools.noki6250_sms_check.DELETED_FRAME", sha256(pixels).hexdigest()):
            verify(LOG + "sim_device: update fid=6f3c record=1 length=176\n",
                   self.record(0), pixels, (96, 60), deleted=True)

    def test_unread_is_not_read(self):
        with self.assertRaisesRegex(ValueError, "storage status"):
            verify(LOG, self.record(3), bytes(96 * 60), (96, 60))

    def test_wrong_frame_rejected(self):
        with self.assertRaisesRegex(ValueError, "outcome frame"):
            verify(LOG, self.record(1), bytes(96 * 60), (96, 60))


if __name__ == "__main__":
    unittest.main()
