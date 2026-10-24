import unittest

from tools.nsb3_puk_check import check_recovery
from tools.nsb3_puk_retry_check import check_rejected_card
from tools.test_nsb3_puk_check import recovery
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


def wrong_trace():
    text = recovery().replace('stage=puk key=Keypad 8 down=', 'stage=puk key=Keypad 9 down=')
    # The synthetic TX changes the final PUK digit, not the replacement PIN.
    lines = [line for line in text.splitlines() if not line.startswith('nsb3_phonecode_')]
    tx = [i for i, line in enumerate(lines) if line.startswith('nsb3_sim_wire:')]
    index = tx[3 + 5 + 7]
    lines[index] = lines[index].replace('data=38', 'data=39')
    return '\n'.join(lines).replace('sw=9000 chv=3/3 puk=10/10', 'sw=9804 chv=0/3 puk=9/10')


class Nsb3PukRetryTest(unittest.TestCase):
    def test_first_wrong_puk_is_distinct_from_recovery(self):
        check_recovery(wrong_trace(), wrong=True)
        with self.assertRaises(ValueError):
            check_recovery(wrong_trace())

    def test_wrong_puk_must_not_claim_phone_unlock_or_reset(self):
        for suffix in ('\nnsb3_phonecode_key: unexpected', '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_recovery(wrong_trace() + suffix, wrong=True)

    def test_complete_card_limits_mutation_to_puk1_retries(self):
        card = bytearray(make_current_profile(True))
        card[ATTEMPTS_OFFSET] = 0
        card[ATTEMPTS_OFFSET + 2] = 9
        check_rejected_card(card)
        for offset in (0, ATTEMPTS_OFFSET, ATTEMPTS_OFFSET + 3):
            changed = bytearray(card)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_rejected_card(changed)


if __name__ == '__main__':
    unittest.main()
