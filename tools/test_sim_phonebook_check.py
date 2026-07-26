import unittest

from tools.sim_phonebook_check import (
    CURRENT_NVRAM_LENGTH,
    RECORD_COUNT,
    RECORD_LENGTH,
    validate_phonebook,
)


class SimPhonebookCheckTest(unittest.TestCase):
    def fixture(self):
        data = bytearray([0xff] * (RECORD_COUNT * RECORD_LENGTH))
        data[0:3] = b"ADA"
        data[18:22] = bytes((0x03, 0x81, 0x21, 0xF3))
        trace = (
            "sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a\n"
            "sim_device: update fid=6f3a record=1 length=32\n"
        )
        return trace, data

    def test_accepts_expected_record(self):
        trace, data = self.fixture()
        validate_phonebook(trace, bytes(data))

    def test_rejects_second_modified_record(self):
        trace, data = self.fixture()
        data[RECORD_LENGTH] = 0
        with self.assertRaisesRegex(ValueError, "more than one"):
            validate_phonebook(trace, bytes(data))

    def test_rejects_missing_update_trace(self):
        _, data = self.fixture()
        with self.assertRaisesRegex(ValueError, "did not issue"):
            validate_phonebook("", bytes(data))

    def test_accepts_current_append_only_card_layout(self):
        trace, data = self.fixture()
        data.extend(bytes(CURRENT_NVRAM_LENGTH - len(data)))
        validate_phonebook(trace, bytes(data))

    def test_rejects_unversioned_card_layout(self):
        trace, data = self.fixture()
        data.append(0)
        with self.assertRaisesRegex(ValueError, "expected one of"):
            validate_phonebook(trace, bytes(data))


if __name__ == "__main__":
    unittest.main()
