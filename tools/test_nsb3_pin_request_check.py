import unittest

from tools.nsb3_pin_request_check import check_trace, SETTINGS_KEYS
from tools.nsb3_pin_check import check_card
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import padded_pin, ENABLED_OFFSET


def trace(phase):
    lines = ['nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
             'nsb3_phonecode_menu: down=1 physical_only=1', 'nsb3_phonecode_menu: down=0 physical_only=1']
    lines += [f'nsb3_phonecode_key: name=Keypad {digit} down={state}' for digit in '12345' for state in (1, 0)]
    tx = bytes.fromhex('ff00ffa0a40000023f00')
    if phase in (0, 3):
        keys = ['Keypad ' + digit for digit in '1234'] + ['Left Softkey / Menu']
        lines += [f'nsb3_pin_key: correct=1 key={key} down={state} t=12.0 physical_only=1'
                  for key in keys for state in (1, 0)]
        tx += bytes.fromhex('a020000108') + padded_pin('1234')
        lines.append('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
    if phase in (0, 2):
        keys = SETTINGS_KEYS + ('Scroll Down' if phase == 0 else 'Scroll Up', 'Left Softkey / Menu')
        lines += [f'nsb3_security_key: key={key} down={state} t=40.0 physical_only=1'
                  for key in keys for state in (1, 0)]
        instruction = 0x26 if phase == 0 else 0x28
        tx += bytes((0xa0, instruction, 0, 1, 8)) + padded_pin('1234')
        lines.append(f'SIM status ins={instruction:02x} sw=9000 chv=3/3 puk=10/10 enabled={phase // 2}')
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=75.0 observer_writes=0' for byte in tx]
    return '\n'.join(lines)


class Nsb3PinRequestTest(unittest.TestCase):
    def test_all_four_phases_have_distinct_authentication_and_mutation(self):
        for phase in range(4):
            check_trace(trace(phase), phase)
            with self.assertRaises(ValueError):
                check_trace(trace(phase), (phase + 1) % 4)

    def test_disabled_cold_must_not_hide_pin_or_extra_card_mutations(self):
        for phase in (1, 2):
            with self.assertRaises(ValueError):
                check_trace(trace(phase) + '\nnsb3_pin_key: unexpected', phase)
            extra = bytes.fromhex('a0d6000000')
            text = trace(phase) + '\n' + '\n'.join(
                f'nsb3_sim_wire: direction=TX data={byte:02x} t=80.0 observer_writes=0' for byte in extra)
            with self.assertRaises(ValueError):
                check_trace(text, phase)

    def test_settings_keys_and_card_enabled_state_are_not_interchangeable(self):
        for phase in (0, 2):
            with self.assertRaises(ValueError):
                check_trace(trace(phase).replace('nsb3_security_key:', 'missing_security_key:'), phase)
        card = make_current_profile(False)
        check_card(card, 3, enabled=False)
        with self.assertRaises(ValueError):
            check_card(card, 3)
        enabled = bytearray(card)
        enabled[ENABLED_OFFSET] = 1
        self.assertEqual(bytes(enabled), make_current_profile(True))


if __name__ == '__main__':
    unittest.main()
