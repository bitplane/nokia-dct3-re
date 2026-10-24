import unittest

from tools.nsb3_fdn_rejection_check import PREFIX, rejected_card, check_rejected_card, check_rejection
from tools.nsb3_fdn_check import expected_card
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import padded_pin


def rejected_trace():
    text = trace(3)
    keys = PREFIX + ('Left Softkey / Menu', 'Scroll Up', 'Left Softkey / Menu') + tuple(
        'Keypad ' + digit for digit in '5679') + ('Left Softkey / Menu',)
    text += '\n' + '\n'.join(f'nsb3_security_key: key={key} down={state} t=76.0 physical_only=1'
                             for key in keys for state in (1, 0))
    tx = bytes.fromhex('a020000208') + padded_pin('5679')
    text += '\n' + '\n'.join(f'nsb3_sim_wire: direction=TX data={byte:02x} t=78.0 observer_writes=0' for byte in tx)
    return text + '\nSIM status ins=20 sw=9804 chv=3/2 puk=10/10 enabled=1'


class Nsb3FdnRejectionTest(unittest.TestCase):
    def test_wrong_pin2_does_not_hide_successful_startup_pin1(self):
        check_rejection(rejected_trace())
        for bad in (rejected_trace().replace('sw=9804', 'sw=9000'),
                    rejected_trace().replace('nsb3_security_key:', 'missing_key:'),
                    rejected_trace() + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_rejection(bad)

    def test_failed_verification_cannot_change_validity(self):
        extra = bytes.fromhex('a0a40000026f3aa004000000')
        text = rejected_trace() + '\n' + '\n'.join(
            f'nsb3_sim_wire: direction=TX data={byte:02x} t=79.0 observer_writes=0' for byte in extra)
        with self.assertRaises(ValueError):
            check_rejection(text)

    def test_whole_card_only_consumes_pin2_retry(self):
        before, after = expected_card(), rejected_card()
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b], [(3517, 3, 2)])
        check_rejected_card(after)
        for offset in (0, 3516, 3575, len(after) - 1):
            changed = bytearray(after)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_rejected_card(changed)


if __name__ == '__main__':
    unittest.main()
