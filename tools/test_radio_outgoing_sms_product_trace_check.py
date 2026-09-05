import pathlib
import tempfile
import unittest
from unittest import mock

from tools.radio_outgoing_sms_product_trace_check import verify


GOOD = """
GSM service establish sapi=0 pd=05 message=24 length=16
TX packet type=1b payload=25 data=00800d3f01
gsm_sms_submit: cp=29 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=5 outcome=0 status_report=0
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5
GSM service uplink sapi=3 pd=09 message=04 length=2 data=2904
LAPDm service Channel Release acknowledged nr=2
"""


class OutgoingSmsProductTraceCheckTest(unittest.TestCase):
    def test_complete_transaction(self):
        with tempfile.TemporaryDirectory() as directory, mock.patch(
                "tools.radio_outgoing_sms_product_trace_check.MESSAGE_SENT_HASHES",
                {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}):
            pathlib.Path(directory, "nokia_dct3_lcdmirror_test.pgm").write_bytes(b"")
            verify(GOOD, pathlib.Path(directory))

    def test_rejects_call_service(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "SMS CM Service Request"):
                verify(GOOD.replace("message=24", "message=24x"), pathlib.Path(directory))

    def test_accepts_nhm5_transaction_shape(self):
        nhm5 = (GOOD.replace("cp=29", "cp=39")
                .replace("alphabet=0", "alphabet=2")
                .replace("status_report=0", "status_report=1")
                .replace("data=2904", "data=3904"))
        with tempfile.TemporaryDirectory() as directory, mock.patch(
                "tools.radio_outgoing_sms_product_trace_check.NHM5_MESSAGE_SENT_HASHES",
                {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}):
            pathlib.Path(directory, "nokia_dct3_lcdmirror_test.pgm").write_bytes(b"")
            verify(nhm5, pathlib.Path(directory), "nhm5")


if __name__ == "__main__":
    unittest.main()
