import unittest
from tools.make_sim_card_profile import make_profile
from tools.sim_fdn_profile import extend
from tools.nsb1_fdn_record_check import card_result, edited_card
from tools.nsb1_fdn_erase_check import erased_card


class Nsb1FdnEraseTests(unittest.TestCase):
    def card(self):
        base = bytearray(make_profile(True))
        base.extend(bytes(3570 - len(base)))
        return edited_card(card_result(extend(base, True)))

    def test_erases_only_selected_record(self):
        before = bytearray(self.card())
        before[3610:3642] = bytes([0x42]) * 32
        after = erased_card(before)
        self.assertEqual(after[:3578], before[:3578])
        self.assertEqual(after[3578:3610], bytes([255]) * 32)
        self.assertEqual(after[3610:], before[3610:])

    def test_wrong_entry_state_or_length_rejected(self):
        before = bytearray(self.card())
        before[3600] = 0xf5
        for card in (before, self.card()[:-1], erased_card(self.card())):
            with self.assertRaises(ValueError):erased_card(card)


if __name__ == '__main__':
    unittest.main()
