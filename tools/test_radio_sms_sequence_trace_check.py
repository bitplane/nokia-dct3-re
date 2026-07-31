import unittest

from tools.radio_sms_sequence_trace_check import (
    FIRST, SECOND, SMS_NVRAM_OFFSET, SMS_RECORD_SIZE, verify)


class SequenceTraceCheckTest(unittest.TestCase):
    def fixture(self):
        log = "\n".join([
            "PCH IMSI page transmitted channel=60",
            "GSM service uplink sapi=3 pd=09 message=01 length=5 "
            "data=8901020240",
            "GSM service downlink kind=17 sapi=3 pd=09 message=04",
            "LAPDm service Channel Release acknowledged nr=3",
            "PCH IMSI page transmitted channel=60",
            "GSM service uplink sapi=3 pd=09 message=01 length=5 "
            "data=8901020241",
            "GSM service downlink kind=17 sapi=3 pd=09 message=04",
            "LAPDm service Channel Release acknowledged nr=3",
        ])
        nvram = bytearray(SMS_NVRAM_OFFSET + 2 * SMS_RECORD_SIZE)
        nvram[SMS_NVRAM_OFFSET] = 3
        nvram[SMS_NVRAM_OFFSET + 1:SMS_NVRAM_OFFSET + 1 + len(FIRST)] = FIRST
        second = SMS_NVRAM_OFFSET + SMS_RECORD_SIZE
        nvram[second] = 3
        nvram[second + 1:second + 1 + len(SECOND)] = SECOND
        return log, bytes(nvram)

    def test_accepts_two_isolated_messages(self):
        verify(*self.fixture())

    def test_rejects_shared_transaction_reference(self):
        log, nvram = self.fixture()
        with self.assertRaisesRegex(ValueError, "reference 41"):
            verify(log.replace("8901020241", "8901020240"), nvram)

    def test_state_mode_requires_successful_replay(self):
        log, nvram = self.fixture()
        with self.assertRaisesRegex(ValueError, "state replay"):
            verify(log, nvram, state=True)
        verify(log + "\nstate_roundtrip: result=pass", nvram, state=True)


if __name__ == "__main__":
    unittest.main()
