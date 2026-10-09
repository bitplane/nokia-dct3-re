import unittest

from tools.run_noki8890_alarm import check_alarm


class AlarmChecks(unittest.TestCase):
    def setUp(self):
        actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings', 'alarm']
        actions += [f'time_{index}' for index in range(1, 5)] + ['confirm']
        self.text = ''.join(f'8890_alarm_physical: action={action}\n' for action in actions)
        self.text += (
            'event=alarm_write reg=0b data=30 armed=1 t=32\n'
            'event=alarm_write reg=0c data=0d armed=1 t=32\n'
            '8890_alarm_physical: action=idle\n'
            'event=second time=13:48:00 day=0 status=b3 t=60\n'
            'event=cause_read data=b3 t=60.001\n'
            'event=status_ack data=a0 old=b3 t=60.002\n'
            'buzzer: enabled=1 t=60.003\n'
            '8890_alarm_physical: action=stop\n'
            'buzzer: enabled=0 t=75\n')

    def test_complete_own_lifecycle(self):
        check_alarm(self.text)

    def test_every_observation_required(self):
        for line in self.text.splitlines(keepends=True):
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_alarm(self.text.replace(line, '', 1))

    def test_wrong_programming_cause_ack_or_deadline(self):
        for old, new in [('reg=0b data=30', 'reg=0b data=31'),
                         ('reg=0c data=0d', 'reg=0c data=0c'),
                         ('data=b3', 'data=33'), ('data=a0', 'data=81'),
                         ('status=b3 t=60', 'status=b3 t=61')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_alarm(self.text.replace(old, new))

    def test_extra_physical_input_rejected(self):
        with self.assertRaises(ValueError):
            check_alarm(self.text + '8890_alarm_physical: action=confirm\n')

    def test_stop_must_follow_expiry_and_ack(self):
        stop = '8890_alarm_physical: action=stop\n'
        with self.assertRaises(ValueError):
            check_alarm(stop + self.text.replace(stop, ''))

    def test_final_buzzer_must_be_off(self):
        with self.assertRaises(ValueError):
            check_alarm(self.text + 'buzzer: enabled=1 t=80\n')

    def test_no_clock_replacement_or_power_wake(self):
        for event in ['8890_clock_physical: key=Keypad 1\n',
                      'ccont_power: event=wake cause=80 t=60\n']:
            with self.subTest(event=event), self.assertRaises(ValueError):
                check_alarm(self.text + event)


if __name__ == '__main__':
    unittest.main()
