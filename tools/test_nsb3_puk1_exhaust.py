import unittest

from tools.nsb3_puk_check import check_recovery
from tools.nsb3_puk_retry_check import check_rejected_card
from tools.test_nsb3_puk_retry_check import wrong_trace
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class Nsb3Puk1ExhaustTest(unittest.TestCase):
    def test_every_remaining_counter_is_an_exact_card_transition(self):
        for remaining in range(11):
            card = bytearray(make_current_profile(True))
            card[ATTEMPTS_OFFSET] = 0
            card[ATTEMPTS_OFFSET + 2] = remaining
            check_rejected_card(card, remaining)
            with self.assertRaises(ValueError):
                check_rejected_card(card, (remaining + 1) % 11)

    def test_last_wrong_puk_requires_terminal_status(self):
        text = wrong_trace().replace('sw=9804 chv=0/3 puk=9/10', 'sw=9840 chv=0/3 puk=0/10')
        check_recovery(text, wrong=True, remaining=0)
        with self.assertRaises(ValueError):
            check_recovery(text, wrong=True, remaining=1)


if __name__ == '__main__':
    unittest.main()
