from unittest.mock import patch
import unittest

from tools import nsm1_clock_cold_check as checker


def fixture():
    lines=[]
    for time,name,code in checker.CONTACTS:
        for down in (1,0):
            lines.append(f'nsm1_clock_cold_contact: name={name} key={code:02x} down={down} t={time+(0 if down else .2):.9f} physical_only=1\n')
    lines += ['ccont_rtc: event=second time=12:35:15 day=0 status=13 mask=50 t=1.000000000\n',
              'ccont_rtc: event=second time=12:36:00 day=0 status=33 mask=50 t=46.000000000\n',
              'ccont_rtc: event=status_ack data=20 old=33 t=46.000247231\n',
              'nsm1_clock_cold_native: pc=099f stop=0 t=61.500000000 observer_writes=0\n']
    return ''.join(lines)


class ColdClockTest(unittest.TestCase):
    def check(self,text):
        with patch.object(checker,'check_authentication'):
            return checker.check_trace(text)

    def test_retained_clock_and_cancelled_editors(self):
        result=self.check(fixture())
        self.assertTrue(result['cold_clock_readback'])
        self.assertFalse(result['clock_reprogrammed'])

    def test_reprogramming_alarm_replay_or_changed_counter_fails(self):
        good=fixture()
        for bad in (good+'ccont_rtc: event=alarm_write reg=0b data=23\n',
                    good+'ccont_rtc: event=counter_write reg=07 data=00\n',
                    good+'buzzer: enabled=1 divider=5285 frequency=2459 volume=3 t=47.000000\n',
                    good.replace('time=12:35:15','time=12:00:15'),
                    good.replace('data=20 old=33','data=00 old=33'),
                    good.replace('stop=0','stop=1'),
                    good.replace('name=Right Softkey','name=Left Softkey'),
                    good+'nsm1_clock_cold_contact: malformed\n'):
            with self.subTest(tail=bad[-90:]),self.assertRaises(ValueError):
                self.check(bad)

    def test_reset_and_authentication_errors_are_not_bypassed(self):
        with patch.object(checker,'check_authentication',side_effect=ValueError('authentication failed')):
            with self.assertRaisesRegex(ValueError,'authentication failed'):
                checker.check_trace(fixture())


if __name__=='__main__':
    unittest.main()
