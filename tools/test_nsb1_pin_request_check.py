import unittest
from tools.nsb1_pin_request_check import card_result, check_trace
from tools.sim_security_trace_check import ENABLED_OFFSET


class Nsb1PinRequestTests(unittest.TestCase):
    def test_complete_card_changes_only_request_bit(self):
        card = bytearray(3570)
        card[ENABLED_OFFSET] = 1
        for phase in range(4):
            result = card_result(card, phase)
            differences = [i for i, (a, b) in enumerate(zip(card, result)) if a != b]
            self.assertEqual(differences, [ENABLED_OFFSET] if phase in (0, 2) else [])
            card = result

    def test_wrong_phase_state_and_length_rejected(self):
        for card, phase in ((bytes(3570), 0), (bytes(3569), 1), (bytes(3570), 4)):
            with self.assertRaises(ValueError):card_result(card, phase)

    def test_disabled_boot_has_phone_code_but_no_sim_verify(self):
        value = '\n'.join(f'nsb1_phonecode_contact: index={i} down={down} t=12.0 physical_only=1'
                          for i in range(1, 7) for down in (1, 0))
        value += '\nnsb1_security_gate: retry=00 match=00000001 t=14.0 observer_writes=0'
        check_trace(value, 1)
        for bad in (value.replace('match=00000001', 'match=00000000'),
                    value + '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=0',
                    value + '\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0',
                    value.replace('index=6 down=1', 'index=7 down=1')):
            with self.assertRaises(ValueError):check_trace(bad, 1)


if __name__ == '__main__':
    unittest.main()
