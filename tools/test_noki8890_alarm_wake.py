import unittest

from tools.run_noki8890_alarm_wake import check_restore, check_wake


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


class AlarmRestoreTest(unittest.TestCase):
    def trace(self):
        tick = 'ccont_rtc: event=second time=13:48:46 day=0 t=46.000000000\n'
        cpu = ','.join(['00000000'] * 37)
        return (f'before\n8890_alarm_state: event=saved pc=00234566 sp=00139000 ram=12345678 cpu={cpu} t=45.000000000\n'
                '8890_alarm_replay: phase=reference event=begin t=45.000000000\n' + tick +
                '8890_alarm_replay: phase=reference event=end t=46.250000000\n'
                f'8890_alarm_state: event=restored pc=00234566 sp=00139000 ram=12345678 cpu={cpu} t=45.000000000\n'
                '8890_alarm_replay: phase=restored event=begin t=45.000000000\n' + tick +
                '8890_alarm_replay: phase=restored event=end t=46.250000000\nafter\n')

    def test_exact_replay(self):
        result = check_restore(self.trace())
        self.assertNotIn('phase=reference', result)
        self.assertTrue(result.startswith('before\n8890_alarm_state: event=restored'))
        self.assertTrue(result.endswith('after\n'))

    def test_architecture_mismatch(self):
        with self.assertRaises(ValueError):
            check_restore(self.trace().replace('event=restored pc=00234566', 'event=restored pc=00234568'))

    def test_missing_tick(self):
        with self.assertRaises(ValueError):
            check_restore(self.trace().replace('ccont_rtc: event=second', 'ccont_rtc: event=other'))

    def test_banked_register_mismatch(self):
        text = self.trace()
        position = text.index('event=restored')
        original = ','.join(['00000000'] * 37)
        changed = ['00000000'] * 37
        changed[17] = '00000001'  # FR8, the first FIQ-banked register.
        with self.assertRaises(ValueError):
            check_restore(text[:position] + text[position:].replace(original, ','.join(changed), 1))

    def test_short_cpu_snapshot(self):
        with self.assertRaises(ValueError):
            check_restore(self.trace().replace('cpu=00000000,', 'cpu=', 1))

    def test_changed_replay_tick(self):
        with self.assertRaises(ValueError):
            check_restore(self.trace().replace('time=13:48:46', 'time=13:48:47', 1))

    def test_endpoint_activity(self):
        with self.assertRaises(ValueError):
            check_restore(self.trace().replace('ccont_rtc: event=second',
                                               'dspif_transport: RX enqueue\nccont_rtc: event=second', 1))

    def test_early_wake(self):
        with self.assertRaises(ValueError):
            check_restore(self.trace().replace('phase=reference event=end',
                                               'ccont_power: event=wake\n8890_alarm_replay: phase=reference event=end'))


if __name__ == '__main__':
    unittest.main()
