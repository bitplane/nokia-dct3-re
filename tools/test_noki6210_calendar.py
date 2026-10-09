import unittest

from tools.run_noki6210_calendar import check_cold, check_entry, check_rtc


class CalendarChecks(unittest.TestCase):
    def setUp(self):
        self.rtc = bytes((19, 47, 13, 0, 0, 30, 2, 48, 1))
        actions = ['menu'] + [f'menu_{i}' for i in range(2, 9)] + ['selected']
        self.cold = ''.join(f'6210_calendar_cold: action={a}\n' for a in actions)
        actions += [f'time_{i}' for i in range(1, 5)] + ['time_confirm']
        actions += [f'date_{i}' for i in range(1, 9)] + ['date_confirm']
        self.entry = ''.join(f'6210_calendar_probe: action={a}\n' for a in actions)
        self.entry += ('ccont_rtc: event=alarm_write reg=0b data=2f armed=1\n'
                       'ccont_rtc: event=alarm_write reg=0c data=8d armed=0\n')

    def test_complete_observations(self):
        check_entry(self.entry, self.rtc)
        check_cold(self.cold, self.rtc)

    def test_missing_and_reordered_digits(self):
        for text in (self.entry.replace('action=date_8', 'action=missing'),
                     self.entry.replace('action=time_1', 'action=time_X')
                         .replace('action=time_2', 'action=time_1')
                         .replace('action=time_X', 'action=time_2')):
            with self.assertRaises(ValueError):
                check_entry(text, self.rtc)

    def test_missing_hardware_programming(self):
        with self.assertRaises(ValueError):
            check_entry(self.entry.replace('data=8d', 'data=80'), self.rtc)

    def test_cold_replacement_rejected(self):
        with self.assertRaises(ValueError):
            check_cold(self.cold + '6210_calendar_probe: action=date_confirm\n', self.rtc)

    def test_cold_navigation_required(self):
        with self.assertRaises(ValueError):
            check_cold(self.cold.replace('action=selected', 'action=missing'), self.rtc)

    def test_corrupt_retained_counter(self):
        for rtc in (self.rtc[:4], bytes((19, 0, 12)) + self.rtc[3:]):
            with self.assertRaises(ValueError):
                check_rtc(rtc)


if __name__ == '__main__':
    unittest.main()
