import unittest

from tools.nsb3_fdn_check import PREFIX, expected_card, check_card, check_trace
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import padded_pin


def fdn_trace(phase):
    text = trace(3)
    keys = PREFIX
    if not phase % 2:
        keys += ('Left Softkey / Menu', 'Scroll Up' if phase == 0 else 'Scroll Down',
                 'Left Softkey / Menu') + tuple('Keypad ' + digit for digit in '5678') + ('Left Softkey / Menu',)
    text += '\n' + '\n'.join(f'nsb3_security_key: key={key} down={state} t=76.0 physical_only=1'
                             for key in keys for state in (1, 0))
    tx = b''
    if not phase % 2:
        tx += bytes.fromhex('a020000208') + padded_pin('5678')
        text += '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
    if phase == 0:
        tx += bytes.fromhex('a0a40000026f3aa004000000')
        text += '\nSIM status ins=04 sw=9000 chv=3/3 puk=10/10 enabled=1'
    for file in ([0x6f7e, 0x6f07], [0x6f7e, 0x6f07], [0x6f7e, 0x6f07, 0x6f3a], [])[phase]:
        tx += bytes.fromhex('a0a4000002') + file.to_bytes(2, 'big') + bytes.fromhex('a044000000')
        text += '\nSIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text + '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=78.0 observer_writes=0' for byte in tx)


class Nsb3FdnTest(unittest.TestCase):
    def test_four_protocol_and_physical_phases_are_distinct(self):
        for phase in range(4):
            check_trace(fdn_trace(phase), phase)
            with self.assertRaises(ValueError):
                check_trace(fdn_trace(phase), (phase + 1) % 4)

    def test_complete_card_changes_only_validity(self):
        off, on = expected_card(), expected_card(True)
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(off, on)) if a != b], [(3575, 1, 0)])
        check_card(on, True)
        for offset in (0, 3517, 3576, len(on) - 1):
            changed = bytearray(on)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_card(changed, True)

    def test_missing_contacts_bad_status_or_reset_fails(self):
        text = fdn_trace(0)
        for bad in (text.replace('nsb3_security_key:', 'missing_key:'),
                    text.replace('ins=04 sw=9000', 'ins=04 sw=9804'),
                    text + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_trace(bad, 0)


if __name__ == '__main__':
    unittest.main()
