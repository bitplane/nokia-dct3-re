import pathlib
import tempfile
import unittest

from tools.radio_network_ussd_trace_check import verify


COMMON = """
PCH IMSI page transmitted channel=60 fn=1
GSM service establish sapi=0 pd=06 message=27 length=11 data=00 t=1
"""
REQUEST = COMMON + """
gsm_ss: network_initiated operation=request transaction=0b invoke=1 dcs=0f t=2
GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29 t=2
input-press: t=2.1 name=enter port=1f
input-press: t=2.2 name=enter port=1f
input-press: t=2.3 name=2 port=1f
input-press: t=2.4 name=enter port=1f
gsm_ss: network_initiated handset_response message=3a length=21 data=8b7a12a210020101300b02013c300604010f040132 t=3
gsm_ss: network_result transaction=8b invoke=1 dcs=0f packed_length=1 empty=0 t=3
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 t=3
LAPDm service Channel Release acknowledged nr=5 t=4
"""
NOTIFY = COMMON + """
gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f t=2
GSM service downlink kind=32 sapi=0 pd=0b message=3b length=35 t=2
gsm_ss: network_initiated handset_response message=3a length=8 data=8b7a05a203020101 t=3
GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 t=3
LAPDm service Channel Release acknowledged nr=6 t=4
"""


class RadioNetworkUssdTraceCheckTest(unittest.TestCase):
    def test_accepts_request_reply(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual({"frames": 0}, verify(
                REQUEST, pathlib.Path(directory), "request"))

    def test_rejects_wrong_reply_invoke(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "request reply"):
                verify(REQUEST.replace("a210020101", "a210020102"),
                       pathlib.Path(directory), "request")

    def test_rejects_reply_without_physical_digit(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "physical request key 2"):
                verify(REQUEST.replace("name=2 port", "name=3 port"),
                       pathlib.Path(directory), "request")

    def test_rejects_uncorrelated_result(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "correlated"):
                verify(REQUEST.replace("network_result transaction=8b", "network_result transaction=1b"),
                       pathlib.Path(directory), "request")

    def test_rejects_mobile_only_version_length(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "request length"):
                verify(REQUEST.replace("length=29", "length=32"),
                       pathlib.Path(directory), "request")

    def test_accepts_notification_without_frame_requirement(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual({"frames": 0}, verify(
                NOTIFY, pathlib.Path(directory), "notify", require_frame=False))

    def test_rejects_missing_network_release(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "RELEASE COMPLETE"):
                verify(NOTIFY.replace("GSM service downlink kind=27", "missing"),
                       pathlib.Path(directory), "notify")


if __name__ == "__main__":
    unittest.main()
