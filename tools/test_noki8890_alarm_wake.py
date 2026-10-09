import unittest

from tools.run_noki8890_alarm_wake import check_wake


class AlarmWakeTest(unittest.TestCase):
    def trace(self):
        compare = '8890_changed_code: event=compare result=00000000 stored=d87d3698 input=d87d3698\n'
        text = '8890_changed_code: event=input bytes=353433323100\n' + compare
        actions = ['menu', 'menu_2', 'menu_3', 'menu_4', 'settings', 'alarm']
        actions += [f'time_{i}' for i in range(1, 5)] + ['confirm']
        text += ''.join(f'8890_alarm_wake_physical: action={a}\n' for a in actions)
        text += 'event=alarm_write reg=0b data=31 armed=1 \n'
        text += 'event=alarm_write reg=0c data=0d armed=1 \n'
        text += ''.join(f'8890_alarm_wake_physical: action={a}\n' for a in ['idle', 'power_off', 'power_release'])
        text += 'ccont_power: event=off t=40.452288231\n'
        text += 'ccont_power: event=wake cause=80 t=60.000000000\n'
        text += 'event=second time=13:49:00 day=0 t=60.000000000\n'
        text += 'event=cause_read data=b3 \nevent=status_ack data=a0 \nbuzzer: enabled=1 \n'
        text += ''.join(f'8890_alarm_wake_physical: action=security_{i}\n' for i in range(1, 7))
        text += 'buzzer: enabled=0 \n8890_changed_code: event=input bytes=353433323100\n' + compare
        text += '8890_alarm_wake_physical: action=stop\n'
        return text

    def test_complete_two_phases(self):
        cold, wake = check_wake(self.trace())
        self.assertNotIn('event=wake', cold)
        self.assertTrue(wake.startswith('ccont_power: event=wake'))

    def test_no_rtc_wake(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace().replace('event=wake', 'event=other'))

    def test_wrong_wake_cause(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace().replace('cause=80', 'cause=02'))

    def test_wrong_deadline(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace().replace('t=60.000000000', 't=61.000000000'))

    def test_rejected_security(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace().replace('result=00000000', 'result=fffffffb'))

    def test_extra_power_event(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace() + 'ccont_power: event=off t=92.000000000\n')

    def test_missing_stop(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace().replace('action=stop', 'action=other'))

    def test_final_buzzer_active(self):
        with self.assertRaises(ValueError):
            check_wake(self.trace() + 'buzzer: enabled=1 \n')


if __name__ == '__main__':
    unittest.main()
