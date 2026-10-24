import unittest

from tools.nsb3_fdn_record_check import WRITE_KEYS, READ_KEYS, check_trace, check_card, expected_card, expected_record, empty_card
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import padded_pin


def record_trace(readback=False):
    text = trace(3)
    keys = READ_KEYS if readback else WRITE_KEYS
    text += '\n' + '\n'.join(f'nsb3_security_key: key={key} down={state} t=79.0 physical_only=1'
                             for key in keys for state in (1, 0))
    tx = bytes.fromhex('a0a40000026f7ea044000000a0a40000026f07a044000000a0a40000026f3ba0b2010420')
    text += '\n' + '\n'.join(['SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1'] * 2)
    if not readback:
        tx += bytes.fromhex('a020000208') + padded_pin('5678')
        text += '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1'
        tx += bytes.fromhex('a0dc010420') + expected_record()
        text += '\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text + '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=86.5 observer_writes=0' for byte in tx)


class Nsb3FdnRecordTest(unittest.TestCase):
    def test_protected_write_and_public_cold_read_are_distinct(self):
        for readback in (False, True):
            check_trace(record_trace(readback), readback)
            with self.assertRaises(ValueError):
                check_trace(record_trace(readback), not readback)

    def test_fdn_is_not_adn_and_update_must_succeed(self):
        text = record_trace()
        for bad in (text.replace('data=3b ', 'data=3a '),
                    text.replace('ins=dc sw=9000', 'ins=dc sw=9804'),
                    text.replace('nsb3_security_key:', 'missing_key:'),
                    text + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_trace(bad)

    def test_card_record_changes_only_fdn_slot(self):
        before, after = empty_card(True), expected_card()
        self.assertEqual(after[:3578], before[:3578])
        self.assertEqual(after[3578:3610], expected_record())
        self.assertEqual(after[3610:], before[3610:])
        check_card(after)
        for offset in (0, 3516, 3575, 3610):
            changed = bytearray(after)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_card(changed)


if __name__ == '__main__':
    unittest.main()
