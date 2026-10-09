import unittest

from tools.run_noki6250_calendar import check_phase


class CalendarTests(unittest.TestCase):
    def rollover(self):
        actions = ['menu', *[f'down_{i}' for i in range(1, 8)], 'calendar',
                   *[f'time_{i}' for i in range(1, 5)], 'time_confirm']
        text = ''.join(f'6250_calendar_physical: action={a}\n' for a in actions)
        text += ('ccont_rtc: event=alarm_write reg=0b data=3b t=42\n'
                 'ccont_rtc: event=alarm_write reg=0c data=97 t=42\n')
        text += ''.join(f'6250_calendar_physical: action=date_{i}\n' for i in range(1, 9))
        return text + (
            '6250_calendar_physical: action=date_confirm\n'
            '6250_calendar_physical: event=entered_presented\n'
            'ccont_rtc: event=second time=00:00:00 day=1 status=31 t=102\n'
            '6250_calendar_physical: action=midnight_back\n'
            'ccont_rtc: event=read reg=0a data=01 t=127\n'
            'ccont_rtc: event=read reg=0a data=00 t=128\n'
            '6250_calendar_physical: action=midnight_reopen\n'
            '6250_calendar_physical: event=midnight_presented\n')

    def test_rollover(self):
        check_phase(self.rollover(), False, True)

    def test_missing_day_consumption_or_midnight(self):
        for token in ('day=1', 'data=01', 'data=00', 'event=midnight_presented'):
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_phase(self.rollover().replace(token, 'invalid'), False, True)

    def test_wrong_entered_time(self):
        with self.assertRaises(ValueError):
            check_phase(self.rollover().replace('data=97', 'data=8d'), False, True)

    def test_cold_rollover_does_not_accept_old_clock(self):
        text = ''.join(f'6250_calendar_physical: action={a}\n'
                       for a in ['menu', *[f'down_{i}' for i in range(1, 8)], 'calendar'])
        text += ('6250_calendar_physical: event=cold_presented\n'
                 'ccont_rtc: event=read reg=09 data=00 t=20\n'
                 'ccont_rtc: event=read reg=08 data=00 t=20\n')
        check_phase(text, True, True)
        with self.assertRaises(ValueError):
            check_phase(text.replace('reg=09 data=00', 'reg=09 data=0d'), True, True)


if __name__ == '__main__':
    unittest.main()
