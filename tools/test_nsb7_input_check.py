import unittest
from tools.nsb7_input_check import check_trace


def trace():
    rows = ['nsb7_profile: machine=nsb7hle observer_writes=0',
            'nsb7_card_fixture: present=0 physical_contact=1',
            'runtime_hle_handoff pc=2c75 native_suspended=1']
    rows += [f'nsb7_startup_write: address=001308b1 value={value} pc=0029ef2c '
             for value in ('00', '08', '0a', '0e', '0f')]
    for key, raw, decoded in [('Left Softkey / Menu', '06', '19'), ('Keypad 1', '07', '01')]:
        rows += ['nsb7_input_regs: phase=before column_mask=20 ctrl=05',
                 f'nsb7_input: key={key} state=1 t=8',
                 'nsb7_key_irq: observer_writes=0',
                 f'nsb7_key_raw: raw={raw} observer_writes=0',
                 f'nsb7_key_decode: key={decoded} observer_writes=0',
                 f'nsb7_input: key={key} state=0 t=9']
    return '\n'.join(rows)


class InputCheckTests(unittest.TestCase):
    def test_complete(self):
        check_trace(trace())

    def test_reject_missing_semantic_evidence(self):
        for line in trace().splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_trace(trace().replace(line, '', 1))

    def test_reject_wrong_profile_mask_key_and_failure(self):
        for old, new in [('machine=nsb7hle', 'machine=noki8290'),
                         ('column_mask=20', 'column_mask=3f'),
                         ('key=01 observer', 'key=02 observer'),
                         ('native_suspended=1', 'native_suspended=0')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(trace().replace(old, new))
        with self.assertRaises(ValueError):
            check_trace(trace() + '\nLUA ERROR')


if __name__ == '__main__':
    unittest.main()
