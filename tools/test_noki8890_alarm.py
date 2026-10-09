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

    def snooze_text(self):
        stop = '8890_alarm_physical: action=stop\n'
        return self.text.replace(stop, (
            '8890_alarm_physical: action=snooze\n'
            'event=alarm_write reg=0b data=36 armed=1 t=75\n'
            'event=alarm_write reg=0c data=0d armed=1 t=75\n'
            'buzzer: enabled=0 t=75\n'
            'event=second time=13:54:00 day=0 status=b3 t=420\n'
            'event=cause_read data=b3 t=420.001\n'
            'event=status_ack data=a0 old=b3 t=420.002\n'
            'buzzer: enabled=1 t=420.003\n' + stop))

    def test_own_snooze_deadline_and_recurrence(self):
        check_alarm(self.snooze_text(), snooze=True)

    def test_snooze_rejects_borrowed_five_minute_deadline(self):
        with self.assertRaises(ValueError):
            check_alarm(self.snooze_text().replace('data=36', 'data=35').replace(
                '13:54:00', '13:53:00'), snooze=True)

    def test_snooze_requires_programming_recurrence_and_physical_stop(self):
        for token in ['event=alarm_write reg=0b data=36 armed=1 t=75\n',
                      'event=alarm_write reg=0c data=0d armed=1 t=75\n',
                      'event=second time=13:54:00 day=0 status=b3 t=420\n',
                      'event=cause_read data=b3 t=420.001\n',
                      'event=status_ack data=a0 old=b3 t=420.002\n',
                      'buzzer: enabled=1 t=420.003\n']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_alarm(self.snooze_text().replace(token, ''), snooze=True)

    def test_snooze_deadline_timestamp_must_match(self):
        with self.assertRaises(ValueError):
            check_alarm(self.snooze_text().replace('status=b3 t=420\n',
                'status=b3 t=421\n'), snooze=True)


if __name__ == '__main__':
    unittest.main()
