import unittest
from tools.nsk1_puk2_check import exhausted_card, recovered_card
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class Nsk1Puk2Tests(unittest.TestCase):
    def test_exhaustion_changes_only_retry(self):
        before = exhaustion_profile()
        for count in (2, 1, 0):
            after = exhausted_card(before)
            self.assertEqual(after[ATTEMPTS_OFFSET+1], count)
            self.assertEqual([i for i, pair in enumerate(zip(before, after)) if pair[0] != pair[1]], [ATTEMPTS_OFFSET+1])
            before = after
        with self.assertRaises(ValueError):
            exhausted_card(before)

    def test_unknown_geometry_is_rejected(self):
        with self.assertRaises(ValueError):
            exhausted_card(exhaustion_profile()[:-1])

    def test_recovery_only_changes_pin2_and_retry(self):
        original = exhaustion_profile()
        blocked = original
        for _ in range(3):
            blocked = exhausted_card(blocked)
        recovered = recovered_card(blocked)
        self.assertEqual([i for i, pair in enumerate(zip(blocked, recovered)) if pair[0] != pair[1]], [3492, 3493, 3494, 3495, 3517])
        self.assertEqual(recovered[ATTEMPTS_OFFSET+1], 3)
        for bad in (original, blocked[:-1]):
            with self.assertRaises(ValueError):
                recovered_card(bad)
        wrong_puk_retry = bytearray(blocked)
        wrong_puk_retry[ATTEMPTS_OFFSET+3] = 9
        with self.assertRaises(ValueError):
            recovered_card(wrong_puk_retry)


if __name__ == '__main__':
    unittest.main()
