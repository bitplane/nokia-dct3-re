import unittest

from tools.nsm1_alarm_power_check import check_power_trace


def trace(activate=False):
    rows=['ccont_power: event=off t=70.776519615']
    rows += [f'ccont_rtc: event=second time=12:34:{second:02d} day=0 status=13 mask=50 t={second+38}.000000000'
             for second in range(33,60)]
    rows += ['ccont_power: event=wake cause=80 t=98.000000000',
             'ccont_rtc: event=status_ack data=a0 old=b3 t=100.237561308']
    rows += [f'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t={101+i/10:.9f}'
             for i in range(20)]
    if not activate:
        rows += ['ccont_power: event=off t=124.979561077',
                 'ccont_rtc: event=second time=12:35:41 day=0 status=13 mask=50 t=139.000000000']
    return '\n'.join(rows)


class PowerAlarmTest(unittest.TestCase):
    def test_both_rail_lifecycles(self):
        for activate in (False,True):
            self.assertEqual(check_power_trace(trace(activate),activate=activate)['activation'],activate)

    def test_wrong_wake_cause(self):
        with self.assertRaisesRegex(ValueError,'alarm wake'):
            check_power_trace(trace().replace('cause=80','cause=01'))

    def test_digital_activity_while_off(self):
        text=trace().replace('ccont_power: event=wake','dspif_transport: FIQ0 notify\nccont_power: event=wake')
        with self.assertRaisesRegex(ValueError,'endpoint active'):
            check_power_trace(text)

    def test_short_final_observation(self):
        with self.assertRaisesRegex(ValueError,'too short'):
            check_power_trace(trace().replace('t=139.000000000','t=128.000000000'))

    def test_missing_retained_tick(self):
        text='\n'.join(row for row in trace().splitlines() if 't=80.000000000' not in row)
        with self.assertRaisesRegex(ValueError,'RTC observation incomplete'):
            check_power_trace(text)

    def test_retained_clock_discontinuity(self):
        with self.assertRaisesRegex(ValueError,'RTC discontinuity'):
            check_power_trace(trace().replace('time=12:34:42','time=12:34:41'))

    def test_unexpected_extra_wake(self):
        with self.assertRaisesRegex(ValueError,'rail transitions'):
            check_power_trace(trace()+'\nccont_power: event=wake cause=01 t=139.500000000')

    def test_activity_after_final_shutdown(self):
        with self.assertRaisesRegex(ValueError,'after No shutdown'):
            check_power_trace(trace()+'\nrom4_port_write: port=0001')

    def test_missing_firmware_acknowledgement(self):
        with self.assertRaisesRegex(ValueError,'acknowledgement'):
            check_power_trace(trace().replace('data=a0 old=b3','data=20 old=b3'))

    def test_stop_does_not_silence(self):
        with self.assertRaisesRegex(ValueError,'Stop lifecycle'):
            check_power_trace(trace()+'\nbuzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=117.000000000')


if __name__=='__main__':
    unittest.main()
