import unittest
from tools.nsk3_fdn_record_check import expected_card, RECORD
from tools.sim_fdn_profile import EXTENDED_LENGTH
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class Nsk3FdnRecordTests(unittest.TestCase):
    def fixture(self):
        card = bytearray([255]) * EXTENDED_LENGTH
        card[3570:3578] = b'FDN\x01\x01\x01\x01\x00'
        card[ATTEMPTS_OFFSET + 1] = 3
        return bytes(card)

    def test_save_changes_only_first_record(self):
        before = self.fixture()
        after = expected_card(before)
        self.assertEqual(after[:3578], before[:3578])
        self.assertEqual(after[3578:3610], RECORD)
        self.assertEqual(after[3610:], before[3610:])

    def test_wrong_pin_changes_only_retry(self):
        before = self.fixture()
        after = expected_card(before, True)
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b],
                         [(ATTEMPTS_OFFSET + 1, 3, 2)])

    def test_nonfresh_inputs_rejected(self):
        for value in (self.fixture()[:-1], expected_card(self.fixture()),
                      expected_card(self.fixture(), True)):
            with self.subTest(value=value), self.assertRaises(ValueError):
                expected_card(value)


if __name__ == '__main__':
    unittest.main()
