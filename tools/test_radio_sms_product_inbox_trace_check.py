import pathlib
import tempfile
import unittest
from unittest import mock

from tools.radio_sms_product_inbox_trace_check import (
    PRODUCT_HASHES, SMS_DELIVER_BODY, SMS_NVRAM_OFFSET, verify)


class ProductInboxTraceCheckTest(unittest.TestCase):
    def test_3410_read(self):
        nvram = bytearray(b"\xff" * 4096)
        nvram[SMS_NVRAM_OFFSET] = 1
        nvram[
            SMS_NVRAM_OFFSET + 1:
            SMS_NVRAM_OFFSET + 1 + len(SMS_DELIVER_BODY)] = SMS_DELIVER_BODY
        with tempfile.TemporaryDirectory() as directory, mock.patch(
                "tools.radio_sms_product_inbox_trace_check._hashes",
                return_value=PRODUCT_HASHES["3410"]["read"]):
            verify(
                "3410", "read",
                "sim_device: update fid=6f3c record=1 length=176",
                pathlib.Path(directory), bytes(nvram))

    def test_rejects_wrong_status(self):
        nvram = bytearray(b"\xff" * 4096)
        with tempfile.TemporaryDirectory() as directory, mock.patch(
                "tools.radio_sms_product_inbox_trace_check._hashes",
                return_value=PRODUCT_HASHES["3410"]["read"]):
            with self.assertRaisesRegex(ValueError, "status"):
                verify(
                    "3410", "read",
                    "sim_device: update fid=6f3c record=1 length=176",
                    pathlib.Path(directory), bytes(nvram))


if __name__ == "__main__":
    unittest.main()
