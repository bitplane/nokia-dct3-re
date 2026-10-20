import unittest
from tools.nsb1_puk2_check import exhausted_card, rejected_puk_card
from tools.nsb1_pin_change_check import check_operation
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


class Nsb1Puk2Tests(unittest.TestCase):
    def test_wrong_puk_preserves_exhausted_pin_and_credentials(self):
        base = bytearray(make_profile(True))
        base.extend(bytes(3570 - len(base)))
        before = exhausted_card(base, 0)
        after = rejected_puk_card(base)
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b],
                         [(ATTEMPTS_OFFSET + 3, 10, 9)])

    def test_successful_unblock_requires_ordered_followup_verify(self):
        value = '\n'.join(f'nsb1_pin_contact: phase={phase} index={i} down={down} t=12.0 physical_only=1'
                          for phase, count in (('pin', 5), ('phone', 6)) for i in range(1, count + 1) for down in (1, 0))
        start = '\nSIM status ins=20 sw=9000 chv=3/0 puk=10/10 enabled=1'
        unblock = '\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1'
        follow = '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        contacts = '\n' + '\n'.join(f'nsb1_pin_change_contact: index={i} down={down} t=26.0 physical_only=1'
                                    for i in range(1, 25) for down in (1, 0))
        check_operation(value + start + unblock + follow + contacts, 0x2c, 0x9000, 24)
        failed = value + start + unblock.replace('sw=9000', 'sw=9804') + contacts
        check_operation(failed, 0x2c, 0x9804, 24)
        with self.assertRaises(ValueError):check_operation(failed, 0x2c, 0x9000, 24)
        for bad in (value + start + unblock + contacts, value + start + follow + unblock + contacts):
            with self.assertRaises(ValueError):check_operation(bad, 0x2c, 0x9000, 24)

    def test_only_pin2_counter_changes(self):
        before = bytearray(make_profile(True))
        before.extend(bytes(3570 - len(before)))
        for remaining in (2, 1, 0):
            after = exhausted_card(before, remaining)
            self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b],
                             [(ATTEMPTS_OFFSET + 1, 3, remaining)])
        for remaining in (-1, 3, 4):
            with self.assertRaises(ValueError):exhausted_card(before, remaining)


if __name__ == '__main__':
    unittest.main()
