import unittest

from tools.nsb3_puk_check import check_recovery
from tools.nsb3_pin_check import check_card
from tools.make_sim_card_profile import make_current_profile
from tools.sim_security_trace_check import padded_pin, CHV_OFFSET


def recovery(p2=0):
    lines = []
    for stage, digits in (('puk', '12345678'), ('new', '2468'), ('repeat', '2468')):
        keys = ['Keypad ' + digit for digit in digits] + ['Left Softkey / Menu']
        lines += [f'nsb3_puk_key: stage={stage} key={key} down={state} t=12.0 physical_only=1'
                  for key in keys for state in (1, 0)]
    tx = bytes.fromhex('ff00ff') + bytes((0xa0, 0x2c, 0, p2, 16)) + b'12345678' + padded_pin('2468')
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=24.0 observer_writes=0' for byte in tx]
    lines += ['SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1',
              'nsb3_phonecode_matched_branch: retry=00 observer_writes=0',
              'nsb3_phonecode_menu: down=1 physical_only=1', 'nsb3_phonecode_menu: down=0 physical_only=1']
    lines += [f'nsb3_phonecode_key: name=Keypad {digit} down={state}' for digit in '12345' for state in (1, 0)]
    return '\n'.join(lines)


class Nsb3PukTest(unittest.TestCase):
    def test_own_p2_zero_unblock_and_separate_phone_authentication(self):
        check_recovery(recovery())
        for bad in (recovery(1), recovery().replace('stage=repeat', 'stage=missing'),
                    recovery().replace('sw=9000', 'sw=9804'),
                    recovery().replace('matched_branch', 'unmatched'),
                    recovery().replace('physical_only=1', 'physical_only=0')):
            with self.assertRaises(ValueError):
                check_recovery(bad)

    def test_replacement_pin_must_preserve_every_other_card_byte(self):
        card = bytearray(make_current_profile(True))
        card[CHV_OFFSET:CHV_OFFSET + 8] = padded_pin('2468')
        check_card(card, 3, '2468')
        with self.assertRaises(ValueError):
            check_card(card, 3)
        card[CHV_OFFSET + 8] ^= 1
        with self.assertRaises(ValueError):
            check_card(card, 3, '2468')

    def test_recovery_must_not_hide_an_extra_verify(self):
        extra = recovery() + '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        with self.assertRaises(ValueError):
            check_recovery(extra)


if __name__ == '__main__':
    unittest.main()
