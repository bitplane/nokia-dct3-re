import unittest

from tools.nsb3_names_options_check import KEYS, check_trace
from tools.test_nsb3_phonebook_check import trace


def options_trace():
    tx = bytes.fromhex('ff00ffa0a40000026f3aa0b2010420')
    return trace(KEYS) + '\n' + '\n'.join(
        f'nsb3_sim_wire: direction=TX data={byte:02x} t=61.0 observer_writes=0' for byte in tx)


class Nsb3NamesOptionsTest(unittest.TestCase):
    def test_own_three_down_route_and_repeat_options_contacts(self):
        check_trace(options_trace())
        with self.assertRaises(ValueError):
            check_trace(options_trace().replace('nsb3_phonebook_key:', 'missing_key:'))

    def test_capacity_audit_is_not_a_contact_write(self):
        extra = bytes.fromhex('a0dc010420') + b'\xff' * 32
        text = options_trace() + '\n' + '\n'.join(
            f'nsb3_sim_wire: direction=TX data={byte:02x} t=62.0 observer_writes=0' for byte in extra)
        with self.assertRaises(ValueError):
            check_trace(text)
        with self.assertRaises(ValueError):
            check_trace(options_trace() + '\nnsb3_board_reset_request: data=05')


if __name__ == '__main__':
    unittest.main()
