import unittest
from tools.nsb7_security_check import check_trace


def trace(accepted=True):
    rows = ['nsb7_profile: machine=nsb7hle observer_writes=0',
            'runtime_hle_handoff pc=2c75 native_suspended=1',
            'nsb7_security_nv_read: offset=09d4 destination=00130a8c length=8',
            'nsb7_security_read: pc=00295a40 data=ffffff',
            'nsb7_security_read: pc=00295a88 data=3132333435 valid=0',
            'nsb7_security_compare: input=313233343500 encoded=d83b30d8 stored=d83b30d8',
            'nsb7_security_retry: match=1 retries=' + ('00' if accepted else 'ff')]
    keys = ['Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Left Softkey / Menu']
    for key, decoded in zip(keys, (1, 2, 3, 4, 5, 0x19)):
        rows += [f'nsb7_security_input: key={key} state=1 t=8',
                 f'nsb7_key_decode: key={decoded:02x} observer_writes=0',
                 f'nsb7_security_input: key={key} state=0 t=9']
    return '\n'.join(rows)


class SecurityCheckTests(unittest.TestCase):
    def test_both_controls(self):
        check_trace(trace())
        check_trace(trace(False), False)
        with self.assertRaises(ValueError):
            check_trace(trace(False))

    def test_missing_evidence_rejected(self):
        for line in trace().splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_trace(trace().replace(line, '', 1))

    def test_wrong_input_and_runtime_failure(self):
        for old, new in [('match=1', 'match=0'), ('encoded=d83b30d8', 'encoded=ffffffff'),
                         ('data=ffffff', 'data=123450')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(trace().replace(old, new))
        with self.assertRaises(ValueError):
            check_trace(trace() + '\nLUA ERROR')


if __name__ == '__main__':
    unittest.main()
