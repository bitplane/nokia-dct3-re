import unittest

from tools.nsb3_puk2_check import check_recovery
from tools.nsb3_puk2_retry_check import check_rejected_card
from tools.test_nsb3_puk2_check import recovery_trace
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


def wrong_trace():
    lines = [line for line in recovery_trace().splitlines() if 't=90.1 ' not in line]
    lines.pop()  # Only the successful replacement VERIFY's final status.
    return '\n'.join(lines).replace('nsb3_security_key: key=Keypad 1 down=', 'nsb3_security_key: key=Keypad 2 down=').replace(
        'data=31 t=90.0', 'data=32 t=90.0').replace('ins=2c sw=9000', 'ins=2c sw=9804')


class Nsb3Puk2RetryTest(unittest.TestCase):
    def test_wrong_puk2_cannot_authenticate_replacement(self):
        check_recovery(wrong_trace(), wrong=True)
        with self.assertRaises(ValueError):
            check_recovery(wrong_trace())
        with self.assertRaises(ValueError):
            check_recovery(recovery_trace(), wrong=True)

    def test_only_puk2_retry_byte_may_change(self):
        card = bytearray(make_current_profile(True))
        card[ATTEMPTS_OFFSET + 1] = 0
        card[ATTEMPTS_OFFSET + 3] = 9
        check_rejected_card(card)
        for offset in (0, ATTEMPTS_OFFSET, ATTEMPTS_OFFSET + 2):
            bad = bytearray(card)
            bad[offset] ^= 1
            with self.assertRaises(ValueError):
                check_rejected_card(bad)


if __name__ == '__main__':
    unittest.main()
