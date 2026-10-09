import unittest

from tools.run_noki6210_alarm import check_alarm, check_snooze, check_power_off_alarm, check_off_restore


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

    def restore_text(self):
        cpu = ','.join(['00000000'] * 37)
        return (
            f'6210_alarm_state: event=saved pc=00123456 sp=00170000 ram=abcdef01 cpu={cpu} t=45.000000000\n'
            '6210_alarm_replay: phase=reference event=begin t=45.000000000\n'
            'ccont_rtc: event=second time=13:47:46 day=0 status=31 t=46.000000000\n'
            '6210_alarm_replay: phase=reference event=end t=46.250000000\n'
            f'6210_alarm_state: event=restored pc=00123456 sp=00170000 ram=abcdef01 cpu={cpu} t=45.000000000\n'
            '6210_alarm_replay: phase=restored event=begin t=45.000000000\n'
            'ccont_rtc: event=second time=13:47:46 day=0 status=31 t=46.000000000\n'
            '6210_alarm_replay: phase=restored event=end t=46.250000000\n')

    def test_off_restore_replays_rtc_and_removes_abandoned_timeline(self):
        result = check_off_restore(self.restore_text())
        self.assertNotIn('event=saved', result)
        self.assertEqual(result.count('ccont_rtc: event=second'), 1)

    def test_snooze_restore_requires_later_checkpoint(self):
        text = self.restore_text().replace('t=45.', 't=100.').replace('t=46.', 't=101.')
        result = check_off_restore(text, checkpoint=100)
        self.assertEqual(result.count('ccont_rtc: event=second'), 1)
        with self.assertRaises(ValueError):
            check_off_restore(text)
        with self.assertRaises(ValueError):
            check_off_restore(self.restore_text(), checkpoint=100)

    def test_off_restore_requires_every_observation(self):
        for line in self.restore_text().splitlines(keepends=True):
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_off_restore(self.restore_text().replace(line, '', 1))

    def test_off_restore_requires_every_register(self):
        original = ','.join(['00000000'] * 37)
        for index in range(37):
            changed = ['00000000'] * 37
            changed[index] = '00000001'
            with self.subTest(register=index), self.assertRaises(ValueError):
                check_off_restore(self.restore_text().replace(original, ','.join(changed), 1))
        with self.assertRaises(ValueError):
            check_off_restore(self.restore_text().replace('cpu=00000000,', 'cpu=', 1))

    def test_off_restore_rejects_state_or_tick_divergence(self):
        for old, new in [('ram=abcdef01', 'ram=abcdef02'),
                         ('time=13:47:46', 'time=13:47:47'),
                         ('event=end t=46.250000000', 'event=end t=47.000000000')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_off_restore(self.restore_text().replace(old, new, 1))

    def test_off_restore_rejects_premature_wake_or_endpoint_activity(self):
        for event in ['ccont_power: event=wake cause=80 t=45.5\n',
                      'dspif_transport: RX enqueue\n']:
            with self.subTest(event=event), self.assertRaises(ValueError):
                check_off_restore(self.restore_text().replace(
                    '6210_alarm_replay: phase=reference event=end', event +
                    '6210_alarm_replay: phase=reference event=end'))

    def power_off_text(self):
        prefix, expiry = self.text.split('event=second time=13:48:00', 1)
        expiry = 'event=second time=13:48:00' + expiry
        expiry = expiry.replace('day=0 status=b1', 'day=0 status=b1 t=60')
        expiry = expiry.replace('event=read reg=0e data=b1', 'event=cause_read data=b1')
        return prefix + (
            '6210_alarm_probe: action=power_off\n'
            '6210_alarm_probe: action=power_release\n'
            'ccont_power: event=off t=38\n'
            'ccont_power: event=wake cause=80 t=60\n'
        ) + expiry + (
            '6210_alarm_probe: action=activate_no\n'
            'ccont_power: event=off t=86\n'
        )

    def test_power_off_alarm_no(self):
        check_power_off_alarm(self.power_off_text(), 'no')

    def power_off_snooze_text(self):
        return self.power_off_text().replace('6210_alarm_probe: action=stop\n',
            '6210_alarm_probe: action=snooze\n'
            'event=alarm_write reg=0b data=35 armed=1 t=78\n'
            'event=alarm_write reg=0c data=0d armed=1 t=78\n'
            'buzzer: enabled=0 t=78\n'
            'ccont_power: event=off t=83\n'
            'ccont_power: event=wake cause=80 t=360\n'
            'event=second time=13:53:00 day=0 status=b1 t=360\n'
            'event=status_ack data=a1 t=363\n'
            'buzzer: enabled=1 t=364\n'
            '6210_alarm_probe: action=stop\n')

    def test_power_off_snooze_no(self):
        check_power_off_alarm(self.power_off_snooze_text(), 'no', snooze=True)

    def test_power_off_snooze_yes(self):
        text = self.power_off_snooze_text().replace('activate_no', 'activate_yes').replace(
            'ccont_power: event=off t=86\n', '')
        check_power_off_alarm(text, 'yes', snooze=True)
        with self.assertRaisesRegex(ValueError, 'Yes activation'):
            check_power_off_alarm(text + 'ccont_power: event=off t=390\n', 'yes', snooze=True)

    def test_power_off_snooze_requires_recurrence_deadline_and_ack(self):
        text = self.power_off_snooze_text()
        for old, new in [('cause=80 t=360', 'cause=80 t=361'),
                         ('data=a1', 'data=81'), ('data=35', 'data=30')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_power_off_alarm(text.replace(old, new), 'no', snooze=True)

    def test_power_off_snooze_rejects_second_off_endpoint_activity(self):
        text = self.power_off_snooze_text().replace(
            'ccont_power: event=off t=83\n',
            'ccont_power: event=off t=83\ndspif_transport: RX enqueue\n')
        with self.assertRaises(ValueError):
            check_power_off_alarm(text, 'no', snooze=True)

    def test_power_off_alarm_rejects_wrong_deadline(self):
        with self.assertRaisesRegex(ValueError, 'natural RTC deadline'):
            check_power_off_alarm(self.power_off_text().replace('cause=80 t=60', 'cause=80 t=61'), 'no')

    def test_power_off_alarm_rejects_extra_wake(self):
        with self.assertRaises(ValueError):
            check_power_off_alarm(self.power_off_text() + 'ccont_power: event=wake cause=01 t=90\n', 'no')

    def test_power_off_alarm_rejects_endpoint_activity(self):
        for text in [self.power_off_text().replace('ccont_power: event=wake',
                     'dspif_transport: RX enqueue\n' + 'ccont_power: event=wake'),
                     self.power_off_text() + 'dspif_transport: RX enqueue\n']:
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_power_off_alarm(text, 'no')

    def test_power_off_alarm_yes_rejects_shutdown(self):
        with self.assertRaisesRegex(ValueError, 'Yes activation'):
            check_power_off_alarm(self.power_off_text().replace('activate_no', 'activate_yes'), 'yes')

    def test_restored_power_off_alarm_yes_continues_without_second_rail_drop(self):
        text = self.power_off_text().replace('activate_no', 'activate_yes').replace(
            'ccont_power: event=off t=86\n', '')
        marker = 'ccont_power: event=wake cause=80 t=60\n'
        text = text.replace(marker, self.restore_text() + marker)
        restored = check_off_restore(text)
        check_power_off_alarm(restored, 'yes')
        with self.assertRaisesRegex(ValueError, 'Yes activation'):
            check_power_off_alarm(restored + 'ccont_power: event=off t=86\n', 'yes')

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

    def cold_text(self):
        expiry = self.text[self.text.index('event=second time=13:48:00'):]
        return ('event=read reg=0b data=30 t=0.06\n'
                'event=read reg=0c data=0d t=0.06\n'
                'event=second time=13:47:01 day=0 status=31\n'
                '6210_alarm_probe: cold_observe=1\n' + expiry)

    def test_cold_lifecycle(self):
        check_alarm(self.cold_text(), cold=True)

    def test_cold_requires_own_observation_and_clock(self):
        for token in ['time=13:47:01', '6210_alarm_probe: cold_observe=1\n',
                      'event=read reg=0b data=30 ', 'event=read reg=0c data=0d ']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_alarm(self.cold_text().replace(token, ''), cold=True)

    def test_cold_rejects_alarm_reentry(self):
        for action in ['confirm', 'time_1']:
            with self.subTest(action=action), self.assertRaises(ValueError):
                check_alarm(self.cold_text() + f'6210_alarm_probe: action={action}\n', cold=True)

    def test_cold_requires_natural_expiry_and_stop(self):
        for line in self.cold_text().splitlines(keepends=True)[4:]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_alarm(self.cold_text().replace(line, '', 1), cold=True)

    def test_cold_rejects_replaced_clock(self):
        with self.assertRaises(ValueError):
            check_alarm(self.cold_text() + '6210_calendar_probe: action=time_confirm\n', cold=True)

    def test_cold_rejects_wrong_first_tick(self):
        with self.assertRaises(ValueError):
            check_alarm('event=second time=00:00:01 day=0 status=31\n' +
                        self.cold_text(), cold=True)

    def snooze_text(self):
        prefix = self.text[:self.text.index('6210_alarm_probe: action=stop\n')]
        return prefix + (
            '6210_alarm_probe: action=snooze\n'
            'event=alarm_write reg=0b data=35 armed=1 t=77\n'
            'event=alarm_write reg=0c data=0d armed=1 t=77\n'
            'buzzer: enabled=0 t=77\n'
            'event=second time=13:53:00 day=0 status=b1\n'
            'event=read reg=0e data=b1 t=360\n'
            'event=status_ack data=a1 old=b1\n'
            'buzzer: enabled=1 t=360\n'
            '6210_alarm_probe: action=stop\n'
            'buzzer: enabled=0 t=377\n')

    def test_snooze_lifecycle(self):
        check_snooze(self.snooze_text())

    def test_snooze_requires_recurrence_delivery(self):
        for token in ['time=13:53:00', 'event=read reg=0e data=b1 t=360\n',
                      'event=status_ack data=a1 old=b1\n', 'buzzer: enabled=1 t=360\n']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_snooze(self.snooze_text().replace(token, ''))

    def test_snooze_requires_physical_input_and_programming(self):
        for token in ['6210_alarm_probe: action=snooze\n',
                      'event=alarm_write reg=0b data=35 armed=1 t=77\n',
                      'event=alarm_write reg=0c data=0d armed=1 t=77\n']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check_snooze(self.snooze_text().replace(token, ''))

    def test_snooze_rejects_reordered_recurrence(self):
        event = 'event=second time=13:53:00 day=0 status=b1\n'
        with self.assertRaises(ValueError):
            check_snooze(event + self.snooze_text().replace(event, ''))

    def test_snooze_rejects_replaced_clock(self):
        with self.assertRaises(ValueError):
            check_snooze(self.snooze_text() + '6210_calendar_probe: action=time_confirm\n')

    def test_snooze_rejects_buzzer_left_enabled(self):
        with self.assertRaises(ValueError):
            check_snooze(self.snooze_text() + 'buzzer: enabled=1 t=378\n')


if __name__ == '__main__':
    unittest.main()
