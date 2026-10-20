import unittest
from tools.nsk3_fdn_erase_check import erased_card
from tools.nsk3_fdn_record_check import RECORD
from tools.sim_fdn_profile import EXTENDED_LENGTH


class Nsk3FdnEraseTests(unittest.TestCase):
    def test_erases_only_selected_record(self):
        before = bytearray(EXTENDED_LENGTH)
        before[3578:3610] = RECORD
        before[3600] = 0xf6
        after = erased_card(before)
        self.assertEqual(after[:3578], before[:3578])
        self.assertEqual(after[3578:3610], bytes([255]) * 32)
        self.assertEqual(after[3610:], before[3610:])

    def test_wrong_record_or_length_rejected(self):
        for before in (bytes(EXTENDED_LENGTH), bytes(EXTENDED_LENGTH - 1)):
            with self.assertRaises(ValueError):
                erased_card(before)


if __name__ == '__main__':
    unittest.main()
