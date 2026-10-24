import unittest

from tools.nsb3_fdn_record_rejection_check import KEYS, check_rejection, check_card
from tools.nsb3_fdn_record_check import expected_card, expected_record
from tools.test_nsb3_fdn_record_check import record_trace
from tools.sim_security_trace_check import padded_pin


def rejected_trace():
    lines = [line for line in record_trace(True).splitlines() if not line.startswith('nsb3_security_key:')]
    lines += [f'nsb3_security_key: key={key} down={state} t=79.0 physical_only=1'
              for key in KEYS for state in (1, 0)]
    tx = bytes.fromhex('a020000208') + padded_pin('5679')
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=81.0 observer_writes=0' for byte in tx]
    lines.append('SIM status ins=20 sw=9804 chv=3/2 puk=10/10 enabled=1')
    return '\n'.join(lines)


class Nsb3FdnRecordRejectionTest(unittest.TestCase):
    def test_denied_add_is_not_success_or_missing_authentication(self):
        text = rejected_trace()
        check_rejection(text)
        for bad in (text.replace('sw=9804', 'sw=9000'),
                    text.replace('nsb3_security_key:', 'missing_key:'),
                    text + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_rejection(bad)

    def test_even_an_identical_record_write_is_forbidden_after_denial(self):
        tx = bytes.fromhex('a0dc010420') + expected_record()
        text = rejected_trace() + '\n' + '\n'.join(
            f'nsb3_sim_wire: direction=TX data={byte:02x} t=82.0 observer_writes=0' for byte in tx)
        with self.assertRaises(ValueError):
            check_rejection(text)

    def test_only_retry_changes_and_contact_survives(self):
        before, after = expected_card(), expected_card(2)
        self.assertEqual([(i, a, b) for i, (a, b) in enumerate(zip(before, after)) if a != b], [(3517, 3, 2)])
        check_card(after, 2)
        for offset in (0, 3575, 3578, 3610):
            changed = bytearray(after)
            changed[offset] ^= 1
            with self.assertRaises(ValueError):
                check_card(changed, 2)


if __name__ == '__main__':
    unittest.main()
