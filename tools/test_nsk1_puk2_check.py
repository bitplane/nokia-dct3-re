import unittest
from tools.nsk1_puk2_check import exhausted_card, recovered_card
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET
from tools.nsk1_pin_change_roundtrip import check_change_contacts, MENU


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
        wrong_puk_retry[ATTEMPTS_OFFSET+3] = 0
        with self.assertRaises(ValueError):
            recovered_card(wrong_puk_retry)

    def test_wrong_puk_only_decrements_retry_and_cold_recovery_restores_it(self):
        blocked = exhaustion_profile()
        for _ in range(3):
            blocked = exhausted_card(blocked)
        rejected = recovered_card(blocked, wrong=True)
        self.assertEqual([i for i, pair in enumerate(zip(blocked, rejected)) if pair[0] != pair[1]], [ATTEMPTS_OFFSET+3])
        self.assertEqual(rejected[ATTEMPTS_OFFSET+3], 9)
        self.assertEqual(recovered_card(rejected), recovered_card(blocked))
        remaining = rejected
        for count in range(8, -1, -1):
            after = recovered_card(remaining, wrong=True)
            self.assertEqual(after[ATTEMPTS_OFFSET+3], count)
            self.assertEqual([i for i, pair in enumerate(zip(remaining, after)) if pair[0] != pair[1]], [ATTEMPTS_OFFSET+3])
            remaining = after
        with self.assertRaises(ValueError):
            recovered_card(remaining, wrong=True)

    def test_wrong_puk_input_is_distinct_from_correct_recovery(self):
        menu = MENU[:]
        menu.insert(-1, 'down')
        phases = {'menu': menu, 'old': ['eight','seven','six','five','four','three','two','two','menu'],
                  'new': ['eight','seven','six','five','menu'], 'confirm': ['eight','seven','six','five','menu']}
        trace = '\n'.join(f'nsk1_pin_change_contact: phase={phase} index={i} key={key} down={edge} t=60.0 physical_only=1'
                          for phase, keys in phases.items() for i, key in enumerate(keys, 1) for edge in (1, 0))
        check_change_contacts(trace, pin2=True, puk2=True, wrong_puk2=True)
        with self.assertRaises(ValueError):
            check_change_contacts(trace, pin2=True, puk2=True)


if __name__ == '__main__':
    unittest.main()
