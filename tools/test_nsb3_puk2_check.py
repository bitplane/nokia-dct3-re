import unittest

from tools.nsb3_puk2_check import check_recovery
from tools.nsb3_pin2_change_check import check_trace as check_change
from tools.test_nsb3_pin2_change_check import pin2_trace
from tools.nsb3_pin_change_check import CHANGE_KEYS
from tools.test_nsb3_pin_request_check import trace
from tools.sim_security_trace_check import padded_pin


def recovery_trace():
    lines = trace(3).splitlines()
    keys = CHANGE_KEYS[:12] + ('Scroll Down', 'Left Softkey / Menu')
    for digits in ('87654321', '8765', '8765'):
        keys += tuple('Keypad ' + digit for digit in digits) + ('Left Softkey / Menu',)
    lines += [f'nsb3_security_key: key={key} down={state} t=79.0 physical_only=1'
              for key in keys for state in (1, 0)]
    command = bytes.fromhex('a02c000210') + b'87654321' + padded_pin('8765')
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=90.0 observer_writes=0' for byte in command]
    lines.append('SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1')
    verify = bytes.fromhex('a020000208') + padded_pin('8765')
    lines += [f'nsb3_sim_wire: direction=TX data={byte:02x} t=90.1 observer_writes=0' for byte in verify]
    lines.append('SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
    return '\n'.join(lines)


class Nsb3Puk2Test(unittest.TestCase):
    def test_unblock_is_not_change_or_pin1(self):
        text = recovery_trace()
        check_recovery(text)
        with self.assertRaises(ValueError):
            check_change(text)

    def test_blocked_change_is_distinct_from_first_rejection(self):
        text = pin2_trace(wrong=True).replace('ins=24 sw=9804', 'ins=24 sw=9840')
        check_change(text, wrong=True, blocked=True)
        with self.assertRaises(ValueError):
            check_change(text, wrong=True)

    def test_missing_contacts_bad_status_and_reset_fail(self):
        text = recovery_trace()
        for bad in (text.replace('nsb3_security_key:', 'missing_key:'),
                    text.replace('ins=2c sw=9000', 'ins=2c sw=9804'),
                    text + '\nnsb3_board_reset_request: data=05'):
            with self.assertRaises(ValueError):
                check_recovery(bad)

    def test_replacement_verify_cannot_be_omitted_or_duplicated(self):
        text = recovery_trace()
        with self.assertRaises(ValueError):
            check_recovery('\n'.join(line for line in text.splitlines() if 't=90.1 ' not in line))
        with self.assertRaises(ValueError):
            check_recovery(text + '\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')


if __name__ == '__main__':
    unittest.main()
