"""NSB-7 physical FDN sequence and complete-card transition controls."""
import unittest
from unittest.mock import patch

from tools.nsb7_fdn_check import PREFIX, check_card, check_trace
from tools.nsb7_pin_check import serialized_profile
from tools.sim_fdn_profile import extend, expected_validity_card


def contacts(phase):
    keys = list(PREFIX)
    if phase % 2 == 0:
        keys += [('Left Softkey / Menu', 0x19),
                 ('Scroll Up', 0x17) if phase == 0 else ('Scroll Down', 0x18),
                 ('Left Softkey / Menu', 0x19)]
        keys += [('Keypad '+str(i), i) for i in (5, 6, 7, 8)]
        keys += [('Left Softkey / Menu', 0x19)]
    return '\n'.join(f'nsb7_menu_input: key={key} state=1 t=26\n'
                     f'nsb7_key_decode: key={code:02x} observer_writes=0\n'
                     f'nsb7_menu_input: key={key} state=0 t=26.2' for key, code in keys)


class FdnTest(unittest.TestCase):
    def test_all_card_phases(self):
        previous = None
        card = extend(serialized_profile())
        for phase in range(4):
            after = expected_validity_card(card, phase)
            check_card(card, after, phase, previous)
            changed = bytearray(after)
            changed[3484] ^= 1
            with self.assertRaises(ValueError):
                check_card(card, bytes(changed), phase, previous)
            previous, card = after, after

    def test_cold_predecessor_required(self):
        card = extend(serialized_profile())
        with self.assertRaises(ValueError):
            check_card(card, card, 3, b'other card')

    def test_physical_sequence_and_composed_checks(self):
        for phase in range(4):
            text = 'raw authentication\n'+contacts(phase)
            with patch('tools.nsb7_fdn_check.check_pin') as pin, patch(
                    'tools.nsb7_fdn_check.check_phone') as phone, patch(
                    'tools.nsb7_fdn_check.check_phase') as protocol:
                check_trace(text, phase)
                pin.assert_called_once_with('raw authentication\n', True)
                phone.assert_called_once_with(text)
                protocol.assert_called_once_with(text, phase, 'nsb7_sim_tx')
                with self.assertRaises(ValueError):
                    check_trace(text.replace('key=19 observer_writes=0', 'key=18 observer_writes=0'), phase)

    def test_missing_authentication_rejected(self):
        with self.assertRaises(ValueError):
            check_trace(contacts(0), 0)


if __name__ == '__main__':
    unittest.main()
