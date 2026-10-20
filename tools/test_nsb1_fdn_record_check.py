import unittest
from tools.make_sim_card_profile import make_profile
from tools.sim_fdn_profile import extend
from tools.sim_security_trace_check import ATTEMPTS_OFFSET
from tools.nsb1_fdn_record_check import card_result, edited_card, RECORD


class Nsb1FdnRecordTests(unittest.TestCase):
    def test_edit_changes_only_the_last_number_digit(self):
        before = card_result(self.card())
        after = edited_card(before)
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b],
                         [(3600, 0xf5, 0xf6)])
        with self.assertRaises(ValueError):edited_card(after)
        with self.assertRaises(ValueError):edited_card(self.card())

    def card(self):
        base = bytearray(make_profile(True))
        base.extend(bytes(3570 - len(base)))
        return extend(base, True)

    def test_save_changes_only_first_record(self):
        before = self.card()
        after = card_result(before)
        self.assertEqual(after[:3578], before[:3578])
        self.assertEqual(after[3578:3610], RECORD)
        self.assertEqual(after[3610:], before[3610:])

    def test_rejected_pin2_changes_only_retry(self):
        before = self.card()
        after = card_result(before, True)
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b],
                         [(ATTEMPTS_OFFSET + 1, 3, 2)])

    def test_nonfresh_card_rejected(self):
        for card in (self.card()[:-1], card_result(self.card()), card_result(self.card(), True)):
            with self.assertRaises(ValueError):card_result(card)


if __name__ == '__main__':
    unittest.main()
