import unittest

from tools.nsb3_pin_change_check import CHANGE_KEYS, check_change
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import padded_pin


def change_trace():
    lines = [line for line in trace(3).splitlines()
             if not line.startswith('nsb3_security_key:')]
    lines += [f'nsb3_security_key: key={key} down={state} t=79.0 physical_only=1'
              for key in CHANGE_KEYS for state in (1, 0)]
    command = bytes.fromhex('a024000110') + padded_pin('1234') + padded_pin('4321')
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=88.0 observer_writes=0'
              for byte in command]
    lines.append('SIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=1')
    return '\n'.join(lines)


class Nsb3PinChangeTest(unittest.TestCase):
    def test_physical_change(self):
        check_change(change_trace())

    def test_reset_is_not_a_successful_qualification(self):
        with self.assertRaises(ValueError):
            check_change(change_trace() + '\nnsb3_board_reset_request: data=05')

    def test_missing_contacts_and_wrong_status_fail(self):
        for old, new in [('nsb3_security_key:', 'missing_key:'),
                         ('ins=24 sw=9000', 'ins=24 sw=9804')]:
            with self.assertRaises(ValueError):
                check_change(change_trace().replace(old, new))


if __name__ == '__main__':
    unittest.main()
