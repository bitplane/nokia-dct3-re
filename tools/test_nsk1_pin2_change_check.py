import unittest
from tools.nsk1_pin2_change_check import expected_card
from tools.nsk1_pin_change_roundtrip import check_change_contacts, MENU
from tools.nsk1_puk_roundtrip import exhaustion_profile
from tools.sim_security_trace_check import CHV_OFFSET


class Nsk1Pin2ChangeTests(unittest.TestCase):
    def test_roundtrip_only_changes_pin2_digits(self):
        original = exhaustion_profile()
        changed = expected_card(original)
        self.assertEqual([i for i, pair in enumerate(zip(original, changed)) if pair[0] != pair[1]], list(range(CHV_OFFSET+8, CHV_OFFSET+12)))
        self.assertEqual(expected_card(changed, True), original)
        for bad in (original[:-1], changed):
            with self.assertRaises(ValueError):
                expected_card(bad)

    def test_pin2_route_and_credential_order(self):
        for restore in (False, True):
            menu = MENU[:]
            menu.insert(-1, 'down')
            old = ['five','six','seven','eight','menu']
            new = ['eight','seven','six','five','menu']
            if restore:
                old, new = new, old
            phases = {'menu': menu, 'old': old, 'new': new, 'confirm': new}
            trace = '\n'.join(f'nsk1_pin_change_contact: phase={phase} index={i} key={key} down={edge} t=60.0 physical_only=1'
                              for phase, keys in phases.items() for i, key in enumerate(keys, 1) for edge in (1, 0))
            check_change_contacts(trace, pin2=True, restore=restore)
            for bad in (trace.replace('phase=confirm', 'phase=new'), trace.split('\n', 1)[1]):
                with self.assertRaises(ValueError):
                    check_change_contacts(bad, pin2=True, restore=restore)


if __name__ == '__main__':
    unittest.main()
