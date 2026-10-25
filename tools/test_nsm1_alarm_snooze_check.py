import unittest

from tools.nsm1_alarm_snooze_check import check_trace


def trace():
    rows=[]
    for time,name,code in ((105,'Right Softkey','1a'),(465,'Left Softkey','19')):
        for down in (1,0):
            rows.append(f'nsm1_alarm_snooze_contact: name={name} key={code} down={down} t={time+(0 if down else .2):.9f} physical_only=1')
    rows += ['ccont_rtc: event=alarm_write reg=0b data=29 armed=1 t=105.021180769',
             'ccont_rtc: event=alarm_write reg=0c data=0c armed=1 t=105.021233385']
    for time,clock in ((98,'12:35:00'),(458,'12:41:00')):
        rows.append(f'ccont_rtc: event=second time={clock} day=0 status=b3 mask=50 t={time}.000000000')
        rows.append(f'ccont_rtc: event=status_ack data=a0 old=b3 t={time+.0003:.9f}')
        rows += [f'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t={time+1+i*.1:.9f}' for i in range(15)]
    rows.append('nsm1_alarm_snooze_native: pc=099f stop=0 t=479.500000000 observer_writes=0')
    return '\n'.join(rows)


class SnoozeTest(unittest.TestCase):
    def test_lifecycle(self):
        self.assertEqual(check_trace(trace())['snoozed_alarm_seconds'],458)

    def test_changed_snooze_program(self):
        with self.assertRaisesRegex(ValueError,'program retained alarm'):
            check_trace(trace().replace('data=29','data=28'))

    def test_missing_second_alarm(self):
        with self.assertRaisesRegex(ValueError,'RTC alarm missing'):
            check_trace(trace().replace('time=12:41:00','time=12:40:00'))

    def test_ringing_during_snooze(self):
        with self.assertRaisesRegex(ValueError,'failed to silence'):
            check_trace(trace()+'\nbuzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=200.000000000')

    def test_extra_contact(self):
        with self.assertRaisesRegex(ValueError,'contacts differ'):
            check_trace(trace()+'\nnsm1_alarm_snooze_contact: extra')

    def test_native_stop(self):
        with self.assertRaisesRegex(ValueError,'native execution'):
            check_trace(trace().replace('stop=0','stop=1'))


if __name__=='__main__':
    unittest.main()
