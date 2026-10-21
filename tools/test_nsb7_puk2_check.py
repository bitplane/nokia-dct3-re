"""PIN2/PUK2 retry isolation, automatic VERIFY and cold-chain acceptance."""
import unittest

from tools.nsb7_pin_change_check import check_change
from tools.nsb7_puk2_check import check_cards, card_state
from tools.test_nsb7_pin_change_check import fixture


def pairs():
    return [(card_state(3), card_state(2)), (card_state(2), card_state(1)),
            (card_state(1), card_state(0)), (card_state(0), card_state(0, 9)),
            (card_state(0, 9), card_state(3, pin='8765')),
            (card_state(3, pin='8765'), card_state(3))]


class Puk2Test(unittest.TestCase):
    def test_recovery_and_failed_puk2(self):
        check_change(fixture(False, pin2=True, unblock=True), False, pin2=True, unblock=True)
        check_change(fixture(True, pin2=True, unblock=True), True, pin2=True, unblock=True)

    def test_every_puk2_retry_and_permanent_status(self):
        for remaining in range(10):
            text = fixture(True, pin2=True, unblock=True).replace('puk=10/9', f'puk=10/{remaining}')
            if remaining == 0:
                text = text.replace('ins=2c sw=9804', 'ins=2c sw=9840')
            with self.subTest(remaining=remaining):
                check_change(text, True, pin2=True, unblock=True, puk_remaining=remaining)
                with self.assertRaises(ValueError):
                    check_change(text.replace(f'puk=10/{remaining}', 'puk=9/10'), True,
                                 pin2=True, unblock=True, puk_remaining=remaining)

    def test_permanent_puk2_requires_blocked_status(self):
        with self.assertRaises(ValueError):
            check_change(fixture(True, pin2=True, unblock=True).replace('puk=10/9', 'puk=10/0'),
                         True, pin2=True, unblock=True, puk_remaining=0)

    def test_success_requires_automatic_new_pin2_verify(self):
        text = fixture(False, pin2=True, unblock=True)
        text = text.replace('nsb7_sim_tx: index=30 value=a0', 'missing automatic VERIFY')
        with self.assertRaises(ValueError):
            check_change(text, False, pin2=True, unblock=True)

    def test_wrong_puk2_must_not_change_pin1_counter(self):
        text = fixture(True, pin2=True, unblock=True).replace('chv=3/0 puk=10/9', 'chv=2/0 puk=10/9')
        with self.assertRaises(ValueError):
            check_change(text, True, pin2=True, unblock=True)

    def test_complete_cold_chain(self):
        check_cards(pairs())

    def test_cold_retry_loss_is_rejected(self):
        chain = pairs()
        chain[4] = (card_state(0), chain[4][1])
        with self.assertRaises(ValueError):
            check_cards(chain)

    def test_other_card_mutation_is_rejected(self):
        chain = pairs()
        initial, result = chain[-1]
        chain[-1] = (initial, b'\x00' + result[1:])
        with self.assertRaises(ValueError):
            check_cards(chain)


if __name__ == '__main__':
    unittest.main()
