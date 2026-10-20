import unittest
from tools.nsb1_pin_change_check import changed_card, check_trace
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import CHV_OFFSET, padded_pin


class Nsb1PinChangeTests(unittest.TestCase):
    def test_wrong_current_pin_is_rejection_not_success(self):
        value = '\n'.join(f'nsb1_pin_contact: phase={phase} index={i} down={down} t=12.0 physical_only=1'
                          for phase, count in (('pin', 5), ('phone', 6)) for i in range(1, count + 1) for down in (1, 0))
        value += '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        value += '\nSIM status ins=24 sw=9804 chv=3/2 puk=10/10 enabled=1\n'
        value += '\n'.join(f'nsb1_pin_change_contact: index={i} down={down} t=26.0 physical_only=1'
                           for i in range(1, 21) for down in (1, 0))
        check_trace(value, rejected=True)
        with self.assertRaises(ValueError):check_trace(value)
        with self.assertRaises(ValueError):check_trace(value.replace('sw=9804', 'sw=9000'), rejected=True)

    def test_pin2_preserves_pin1_and_all_other_bytes(self):
        before = bytearray(make_profile(True))
        before.extend(bytes(3570 - len(before)))
        after = changed_card(before, True)
        offset = CHV_OFFSET + 8
        self.assertEqual(after[:offset], before[:offset])
        self.assertEqual(after[offset:offset + 8], padded_pin('8765'))
        self.assertEqual(after[offset + 8:], before[offset + 8:])

    def test_only_pin1_is_changed(self):
        before = bytearray(make_profile(True))
        before.extend(bytes(3570 - len(before)))
        after = changed_card(before)
        self.assertEqual(after[:CHV_OFFSET], before[:CHV_OFFSET])
        self.assertEqual(after[CHV_OFFSET:CHV_OFFSET + 8], padded_pin('4321'))
        self.assertEqual(after[CHV_OFFSET + 8:], before[CHV_OFFSET + 8:])
        with self.assertRaises(ValueError):changed_card(after)

    def test_change_is_not_unblock_or_repeated_after_cold_boot(self):
        value = '\n'.join(f'nsb1_pin_contact: phase={phase} index={i} down={down} t=12.0 physical_only=1'
                          for phase, count in (('pin', 5), ('phone', 6)) for i in range(1, count + 1) for down in (1, 0))
        value += '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        check_trace(value, True)
        for bad in (value + '\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1',
                    value + '\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
                    value.replace('sw=9000', 'sw=9804')):
            with self.assertRaises(ValueError):check_trace(bad, True)


if __name__ == '__main__':
    unittest.main()
