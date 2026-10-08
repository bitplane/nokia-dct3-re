import unittest

from tools.noki8890_cold_clock_check import verify


TEXT = ('8890_clock_nv_result: result=00000001 flags=00 caller=00304ce9\n'
        '8890_clock_nv_cache: kind=app_write pc=002dffdc address=00137414 '
        'data=2a000000 mask=ff000000 t=1.178\n')
STORED = bytes((19, 47, 13, 0, 0, 30, 11, 0x50, 1))


class ColdClockTest(unittest.TestCase):
    def test_own_record_and_controller_validation(self):
        verify(TEXT, STORED)

    def test_missing_invalid_or_injected_settlement_fails(self):
        for text in (TEXT.replace('result=00000001', 'result=00000000'),
                     TEXT.replace('data=2a000000', 'data=34000000'),
                     TEXT + '8890_clock_physical: key=Menu\n',
                     TEXT + 'LUA ERROR\n'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify(text, STORED)

    def test_counter_only_wrong_time_or_snapshot_fails(self):
        for stored in (STORED[:4], bytes((19, 0, 12)) + STORED[3:],
                       STORED[:-1] + b'\x00'):
            with self.subTest(stored=stored), self.assertRaises(ValueError):
                verify(TEXT, stored)


if __name__ == '__main__':
    unittest.main()
