import unittest

from tools.run_noki6210_alarm import check_alarm


class AlarmChecks(unittest.TestCase):
    def setUp(self):
        actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings', 'alarm']
        actions += [f'time_{i}' for i in range(1, 5)] + ['confirm']
        self.text = ''.join(f'6210_alarm_probe: action={a}\n' for a in actions)
        self.text += ('event=alarm_write reg=0b data=30 armed=1 t=30\n'
                      'event=alarm_write reg=0c data=0d armed=1 t=30\n'
                      '6210_alarm_probe: action=idle\n'
                      'event=second time=13:48:00 day=0 status=b1\n'
                      'event=read reg=0e data=b1 t=60\n'
                      'event=status_ack data=81 old=b1\n'
                      'buzzer: enabled=1 t=60\n'
                      '6210_alarm_probe: action=stop\n'
                      'buzzer: enabled=0 t=77\n')

    def test_complete_lifecycle(self):
        check_alarm(self.text)

    def test_each_required_event(self):
        for line in self.text.splitlines(keepends=True):
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_alarm(self.text.replace(line, '', 1))

    def test_wrong_alarm_cause(self):
        with self.assertRaises(ValueError):
            check_alarm(self.text.replace('data=b1', 'data=31'))

    def test_wrong_time_and_programming(self):
        for old, new in [('data=30', 'data=31'), ('data=0d', 'data=0c'),
                         ('13:48:00', '13:49:00')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_alarm(self.text.replace(old, new))

    def test_reordered_stop(self):
        stop = '6210_alarm_probe: action=stop\n'
        with self.assertRaises(ValueError):
            check_alarm(stop + self.text.replace(stop, ''))

    def test_buzzer_reenabled(self):
        with self.assertRaises(ValueError):
            check_alarm(self.text + 'buzzer: enabled=1 t=78\n')

    def test_replaced_clock(self):
        with self.assertRaises(ValueError):
            check_alarm(self.text + '6210_calendar_probe: action=time_confirm\n')


if __name__ == '__main__':
    unittest.main()
