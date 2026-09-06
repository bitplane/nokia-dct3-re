import pathlib
import tempfile
import unittest

from tools import radio_ems_trace_check as checker


class RadioEmsTraceCheckTest(unittest.TestCase):
    def test_accepts_exact_transport_and_record(self):
        nvram = bytearray(checker.SMS_NVRAM_OFFSET + 176)
        start = checker.SMS_NVRAM_OFFSET
        nvram[start:start + len(checker.FORMATTED_RECORD_PREFIX)] = (
            checker.FORMATTED_RECORD_PREFIX)
        checker.verify(
            "PCH IMSI page transmitted channel=60\n"
            "sim_device: update fid=6f3c record=1 length=176\n",
            bytes(nvram))

    def test_rejects_modified_udh(self):
        nvram = bytearray(checker.SMS_NVRAM_OFFSET + 176)
        start = checker.SMS_NVRAM_OFFSET
        nvram[start:start + len(checker.FORMATTED_RECORD_PREFIX)] = (
            checker.FORMATTED_RECORD_PREFIX)
        nvram[start + 28] ^= 1
        with self.assertRaises(ValueError):
            checker.verify(
                "PCH IMSI page transmitted channel=60\n"
                "sim_device: update fid=6f3c record=1 length=176\n",
                bytes(nvram))

    def test_accepts_plain_and_malformed_exact_records(self):
        for profile, prefix in checker.RECORD_PREFIXES.items():
            nvram = bytearray(checker.SMS_NVRAM_OFFSET + 176)
            start = checker.SMS_NVRAM_OFFSET
            nvram[start:start + len(prefix)] = prefix
            checker.verify(
                "PCH IMSI page transmitted channel=60\n"
                "sim_device: update fid=6f3c record=1 length=176\n",
                bytes(nvram), profile)


if __name__ == "__main__":
    unittest.main()
