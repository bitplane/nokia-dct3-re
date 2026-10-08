import unittest

from tools.noki8890_cold_clock_check import verify, verify_minute


TEXT = ('8890_clock_nv_result: result=00000001 flags=00 caller=00304ce9\n'
        '8890_clock_nv_cache: kind=app_write pc=002dffdc address=00137414 '
        'data=2a000000 mask=ff000000 t=1.178\n')
STORED = bytes((19, 47, 13, 0, 0, 30, 11, 0x50, 1))


class ColdClockTest(unittest.TestCase):
    def test_minute_rollover_requires_service_and_late_observation(self):
        text = ('ccont_rtc: event=second time=13:47:59 day=0 status=13 mask=50 t=59.000000000\n'
                'ccont_rtc: event=second time=13:48:00 day=0 status=33 mask=50 t=60.000000000\n'
                'ccont_rtc: event=second time=13:48:01 day=0 status=13 mask=50 t=61.000000000\n'
                '8890_clock_cold: t=90\n')
        verify_minute(text)
        for wrong in (text.replace('13:48:00', '13:47:00'),
                      text.replace('status=33', 'status=13'),
                      text.replace('13:48:01 day=0 status=13', '13:48:01 day=0 status=33'),
                      text.replace('t=90', 't=30')):
            with self.subTest(text=wrong), self.assertRaises(ValueError):
                verify_minute(wrong)

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
