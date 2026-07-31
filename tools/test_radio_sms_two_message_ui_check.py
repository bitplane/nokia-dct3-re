import unittest
from unittest import mock

from tools.radio_sms_two_message_ui_check import (
    FIRST, HASHES, SECOND, SMS_NVRAM_OFFSET, SMS_RECORD_SIZE, verify)


class TwoMessageUiCheckTest(unittest.TestCase):
    def test_selective_delete(self):
        data = bytearray(b"\xff" * 4096)
        second = SMS_NVRAM_OFFSET + SMS_RECORD_SIZE
        data[SMS_NVRAM_OFFSET] = 0
        data[SMS_NVRAM_OFFSET + 1:SMS_NVRAM_OFFSET + 1 + len(FIRST)] = FIRST
        data[second] = 3
        data[second + 1:second + 1 + len(SECOND)] = SECOND
        log = "\n".join((
            "PCH IMSI page transmitted channel=60",
            "data=8901020240",
            "PCH IMSI page transmitted channel=60",
            "data=8901020241",
        ))
        with mock.patch(
                "tools.radio_sms_two_message_ui_check._frame_hashes",
                return_value=HASHES["selective-delete"]):
            verify(
                log, mock.sentinel.snapshots, bytes(data),
                "selective-delete")


if __name__ == "__main__":
    unittest.main()
