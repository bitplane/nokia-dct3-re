import unittest

from tools.radio_sms_negative_trace_check import verify
from tools.radio_sms_acceptance_common import (
    FIRST_SMS_DELIVER_BODY as SMS_DELIVER_BODY,
    SMS_NVRAM_OFFSET,
    SMS_RECORD_SIZE,
)


class SmsNegativeTraceCheckTest(unittest.TestCase):
    def empty_sim(self):
        data = bytearray(b"\xff" * 4096)
        for record in range(10):
            data[SMS_NVRAM_OFFSET + record * SMS_RECORD_SIZE] = 0
        return data

    def test_malformed_never_reaches_handset(self):
        verify(
            "GSM incoming service rejected before paging service=2",
            bytes(self.empty_sim()), "malformed")

    def test_duplicate_is_idempotent(self):
        data = self.empty_sim()
        data[SMS_NVRAM_OFFSET] = 3
        data[
            SMS_NVRAM_OFFSET + 1:
            SMS_NVRAM_OFFSET + 1 + len(SMS_DELIVER_BODY)] = SMS_DELIVER_BODY
        verify("\n".join((
            "PCH IMSI page transmitted channel=60",
            "sim_device: update fid=6f3c record=1 length=176",
            "duplicate queued rp_reference=40 suppressed",
        )), bytes(data), "duplicate")

    def test_duplicate_second_record_fails(self):
        data = self.empty_sim()
        data[SMS_NVRAM_OFFSET] = 3
        data[
            SMS_NVRAM_OFFSET + 1:
            SMS_NVRAM_OFFSET + 1 + len(SMS_DELIVER_BODY)] = SMS_DELIVER_BODY
        data[SMS_NVRAM_OFFSET + SMS_RECORD_SIZE] = 3
        with self.assertRaisesRegex(ValueError, "second record"):
            verify("\n".join((
                "PCH IMSI page transmitted channel=60",
                "sim_device: update fid=6f3c record=1 length=176",
                "duplicate queued rp_reference=40 suppressed",
            )), bytes(data), "duplicate")

    def test_capacity_boundary(self):
        data = self.empty_sim()
        lines = []
        for index, reference in enumerate(range(0x40, 0x4a)):
            start = SMS_NVRAM_OFFSET + index * SMS_RECORD_SIZE
            data[start] = 3
            data[start + 1:start + 1 + len(SMS_DELIVER_BODY)] = (
                SMS_DELIVER_BODY)
            data[start + 20] = index
            lines.extend((
                "PCH IMSI page transmitted channel=60",
                "sim_device: update fid=6f3c record=1 length=176",
                f"data=89010202{reference:02x}",
                "GSM service downlink kind=17 sapi=3 pd=09 message=04",
                "LAPDm service Channel Release acknowledged nr=3",
            ))
        lines.extend((
            "PCH IMSI page transmitted channel=60",
            "data=890104044a0116",
            "GSM service downlink kind=17 sapi=3 pd=09 message=04",
            "LAPDm service Channel Release acknowledged nr=3",
        ))
        verify("\n".join(lines), bytes(data), "capacity")


if __name__ == "__main__":
    unittest.main()
